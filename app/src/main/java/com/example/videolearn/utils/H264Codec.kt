package com.example.videolearn.utils

import com.example.videolearn.video.*
import java.io.*
import java.util.LinkedList

class H264Codec(private val filePath: File) {
    data class ResultData(
        val resultData: ByteArray,
        var unit_type: Int = 0,
        var slice_type: Int = 0,
        var width: Int = 0,
        var height: Int = 0
    ) {
        fun isSps() = unit_type == NAL_SPS
        fun isPPS() = unit_type == NAL_PPS
        fun isI() = unit_type == NAL_I
        fun isB() = slice_type == SLICE_TYPE_B
        fun isP() = slice_type == SLICE_TYPE_P
        override fun toString(): String {
            return "ResultData( unit_type=$unit_type, slice_type=$slice_type, width=$width, height=$height, resultData=${resultData.contentToString()})"
        }
    }

    init {
        println("read file:${filePath.absolutePath}")
    }

    private val resultList = LinkedList<ResultData>()
    fun startCodec(): List<ResultData>? = getFileByte(filePath.absolutePath)?.let { bytes ->
        var startByteIndex = 0
        var totalSize = bytes.size
        var copyStart = -1
        println("totalSize:$totalSize")
        while (startByteIndex < totalSize) {
            val result = findByFrame(bytes, startByteIndex, totalSize)
            if (result.first == -1) {
                println("findByFrame result -1!!!startByteIndex:${startByteIndex}")
                break
            }

            if (copyStart != -1) {
                val byteArray = ByteArray(result.first - copyStart)
                System.arraycopy(bytes, copyStart, byteArray, 0, byteArray.size)
                resultList.add(ResultData(byteArray))
            }
            currReadBit = result.second * 8
            copyStart = result.first
            startByteIndex = result.second

        }
        resultList.forEach {
            val bytes = it.resultData
            currReadBit = findByFrame(bytes, 0, bytes.size).second * 8
            val forbidden_zero_bit = u(1, bytes)
            if (forbidden_zero_bit == 1) {
                //可丢弃
                return@forEach
            }
            val nal_ref_idc = u(2, bytes)
            val nal_unit_type = u(5, bytes)
            println("nal_unit_type:${nal_unit_type}")
            it.unit_type = nal_unit_type
            when (nal_unit_type) {
                NAL_SPS -> {
                    //sps
                    parseSPS(bytes, it)
                }
                NAL_PPS -> {
                    //pps
                    parsePPS(bytes, it)
                }
                NAL_PB -> {
                    parsePB(bytes, it)
                }
                else -> {

                }
            }
        }
        resultList
    } ?: kotlin.run { null }

    private fun parsePB(bytes: ByteArray, resultData: ResultData) {
        val first_mb_in_slice = ue(bytes)
        val slice_type = ue(bytes)
        resultData.slice_type = slice_type % 5
        when (resultData.slice_type) {
            SLICE_TYPE_P -> {
                println("P帧")
            }
            SLICE_TYPE_B -> {
                println("B帧")
            }
            SLICE_TYPE_I -> {
                println("I帧")
            }
            else -> {
                println("其他帧")
            }
        }
    }


    private fun parsePPS(bytes: ByteArray, resultData: ResultData) {


    }

    private fun parseSPS(bytes: ByteArray, resultData: ResultData) {
        val profile_idc = u(8, bytes)
        val constraint_set0_flag = u(1, bytes)
        val constraint_set1_flag = u(1, bytes)
        val constraint_set2_flag = u(1, bytes)
        val constraint_set3_flag = u(1, bytes)
        val reserved_zero_4bits = u(4, bytes)
        val level_idc = u(8, bytes)
        val seq_parameter_set_id = ue(bytes)
        var residual_colour_transform_flag = 0
        var chroma_format_idc = 1
        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 144) {
            chroma_format_idc = ue(bytes)
            if (chroma_format_idc == 3) {
                residual_colour_transform_flag = u(1, bytes)
            }
            val bit_depth_luma_minus8 = ue(bytes)
            val bit_depth_chroma_minus8 = ue(bytes)
            val qpprime_y_zero_transform_bypass_flag = u(1, bytes)
            val seq_scaling_matrix_present_flag = u(1, bytes)
            if (seq_scaling_matrix_present_flag != 0) {
                //需要补充代码,否则条件成立的话会导致数据读取错误
                println("seq_scaling_matrix_present_flag != 0")
            }

        }
        val log2_max_frame_num_minus4 = ue(bytes)
        val pic_order_cnt_type = ue(bytes)
        if (pic_order_cnt_type == 0) {
            val log2_max_pic_order_cnt_lsb_minus4 = ue(bytes)
        } else if (pic_order_cnt_type == 1) {
            //需要补充代码,否则条件成立的话会导致数据读取错误
            println("pic_order_cnt_type == 1")
        }
        val num_ref_frames = ue(bytes)
        val gaps_in_frame_num_value_allowed_flag = u(1, bytes)
        val pic_width_in_mbs_minus1 = ue(bytes)
        val pic_height_in_map_units_minus1 = ue(bytes)
        val frame_mbs_only_flag = u(1, bytes)
        if (frame_mbs_only_flag == 0) {
            val mb_adaptive_frame_field_flag = u(1, bytes)
        }
        val direct_8x8_inference_flag = u(1, bytes)
        val frame_cropping_flag = u(1, bytes)

        var frame_crop_left_offset = 0
        var frame_crop_right_offset = 0
        var frame_crop_top_offset = 0
        var frame_crop_bottom_offset = 0
        if (frame_cropping_flag != 0) {
            frame_crop_left_offset = ue(bytes)
            frame_crop_right_offset = ue(bytes)
            frame_crop_top_offset = ue(bytes)
            frame_crop_bottom_offset = ue(bytes)
        }

        //计算宽高
        var width = (pic_width_in_mbs_minus1 + 1) * 16
        var height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16
        if (frame_cropping_flag != 0) {
            val chromaArrayType = if (residual_colour_transform_flag == 0) {
                chroma_format_idc
            } else {
                0
            }
            var subWidthC = 1
            var subHeightC = 1
            when (chromaArrayType) {
                1 -> {
                    subWidthC = 2
                    subHeightC = 2
                }
                2 -> {
                    subWidthC = 2
                    subHeightC = 1
                }
                3 -> {
                    subWidthC = 1
                    subHeightC = 1
                }
            }
            var crop_unit_x = 0
            var crop_unit_y = 0
            if (chromaArrayType == 0) {
                crop_unit_x = 1
                crop_unit_y = 2 - frame_mbs_only_flag
            } else if (chromaArrayType == 1 || chromaArrayType == 2 || chromaArrayType == 3) {
                crop_unit_x = subWidthC
                crop_unit_y = subHeightC * (2 - frame_mbs_only_flag)
            }
            width -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset)
            height -= crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset)
        }
        println("width:${width} height:${height}")
        resultData.width = width
        resultData.height = height
    }

    /**
     * 获取分隔符占几个字节
     */
    private fun findByFrame(h264: ByteArray, start: Int, totalSize: Int): Pair<Int, Int> {
        for (i in start until totalSize) {
            if (h264[i].toInt() == 0x00
                && h264[i + 1].toInt() == 0x00
                && h264[i + 2].toInt() == 0x00
                && h264[i + 3].toInt() == 0x01
            ) {
                return i to i + 1 + 3
            } else if (h264[i].toInt() == 0x00
                && h264[i + 1].toInt() == 0x00
                && h264[i + 2].toInt() == 0x01
            ) {
                return i to i + 1 + 2
            }
        }
        return -1 to -1// Not found
    }

    companion object {

        private var currReadBit = 0
        fun ue(bytes: ByteArray): Int {
            var zeroNum = 0
            //计算有多少个0
            while (currReadBit < bytes.size * 8) {
                //没有到末尾一直读
                if (bytes[currReadBit / 8].toInt() and (0x80 shr (currReadBit % 8)) != 0) {
                    //
                    break
                }
                zeroNum += 1
                currReadBit += 1
            }
            currReadBit += 1
            var dwRet = 0
            for (i in 0 until zeroNum) {
                dwRet = dwRet shl 1
                if (bytes[currReadBit / 8].toInt() and (0x80 shr (currReadBit % 8)) != 0) {
                    dwRet += 1
                }
                currReadBit += 1
            }
            return (1 shl zeroNum) - 1 + dwRet
        }

        fun u(bitIndex: Int, bytes: ByteArray): Int {
            var dwRet = 0
            for (i in 0 until bitIndex) {
                dwRet = dwRet shl 1
                if (bytes[currReadBit / 8].toInt() and (0x80 shr (currReadBit % 8)) != 0) {
                    dwRet += 1
                }
                currReadBit += 1
            }
            return dwRet
        }

        fun getFileByte(path: String): ByteArray? = kotlin.runCatching {
            val `is`: InputStream = DataInputStream(FileInputStream(File(path)))
            val len: Int
            val size = 1024 * 1024
            val bos = ByteArrayOutputStream()
            var buf = ByteArray(size)
            len = `is`.read(buf, 0, size)
            bos.write(buf, 0, len)
            buf = bos.toByteArray()
            bos.close()
            buf
        }.getOrElse {
            it.printStackTrace()
            null
        }
    }


}