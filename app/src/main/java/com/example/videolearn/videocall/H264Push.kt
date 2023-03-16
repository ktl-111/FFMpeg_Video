package com.example.videolearn.videocall

import android.content.Context
import android.hardware.Camera
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log
import android.view.SurfaceView
import com.example.videolearn.shotscreen.service.SocketPush
import com.example.videolearn.utils.YuvUtils
import java.nio.ByteBuffer

class H264Push(private val context: Context, private val surfaceView: SurfaceView) {
    private val TAG = "H264Push"
    var push: SocketPush? = null
    var camera: Camera? = null
    var width = 0
    var height = 0
    lateinit var mediaCodec: MediaCodec
    fun startCamera() {
        camera = Camera.open(Camera.CameraInfo.CAMERA_FACING_BACK).apply {
//            parameters.setPictureSize(WIDTH, HEIGHT)
            val previewSize = parameters.previewSize
            width = previewSize.width
            height = previewSize.height
            //设置画布
            setPreviewDisplay(surfaceView.holder)
            //预览旋转90度
            setDisplayOrientation(90)
            //420
            val byteArray = ByteArray(width * height * 3 / 2)
            addCallbackBuffer(byteArray)
            setPreviewCallbackWithBuffer { data, camera ->
                encodeFrame(data)
                camera.addCallbackBuffer(data)
            }

            startPreview()

            mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
                .apply {
                    val mediaFormat =
                        MediaFormat.createVideoFormat(
                            MediaFormat.MIMETYPE_VIDEO_AVC,
                            /**
                             * 宽高调换
                             * 因为camera返回的数据是横向的,所以拿到后需要顺时针旋转
                             * 由于旋转后的数据要交给mediacodec去解析,此时就需要将宽高调换
                             * 否则设置的宽就会去解析高
                             *
                             * */
                            height,
                            width
                        )
                    mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, WIDTH * HEIGHT)
                    mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, RATE)
                    mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 5) //IDR帧刷新时间
                    mediaFormat.setInteger(
                        MediaFormat.KEY_COLOR_FORMAT,
                        MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
                    )
                    configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
                    start()
                }
        }
    }

    var index = 0
    private fun encodeFrame(data: ByteArray) {
        //android的数据是nv21,需要转成nv12
        val nv12 = YuvUtils.nv21toNV12(data)
        //数据旋转90度
        val yuv = ByteArray(width * height * 3 / 2)
        /**注意!!!!前置摄像头数据需要逆时针旋转*/
        YuvUtils.portraitData2Raw(nv12, yuv, width, height)
        kotlin.runCatching {
            /*============输入===============*/
            //获取输入空闲的buff index
            val inputIndex = mediaCodec.dequeueInputBuffer(10_000)
            if (inputIndex >= 0) {
                //获取到buffer
                val inputBuffer = mediaCodec.getInputBuffer(inputIndex)
                inputBuffer?.also {
                    it.clear()
                    //设置数据
                    it.put(yuv)
                }
                //通知dsp开始解码
                mediaCodec.queueInputBuffer(inputIndex, 0, yuv.size, computPts(), 0)
                index++
            }

            /*============输出===============*/
            val bufferInfo = MediaCodec.BufferInfo()
            //获取已编码的buffer index
            val outputIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
            if (outputIndex >= 0) {
                //拿到对应buffer
                val outputBuffer = mediaCodec.getOutputBuffer(outputIndex)
                outputBuffer?.also {
                    parseData(it, bufferInfo)
                }
                mediaCodec.releaseOutputBuffer(outputIndex, false)
            }
        }.onFailure {
            it.printStackTrace()
        }
    }

    var spsData: ByteArray? = null
    private fun parseData(outputBuffer: ByteBuffer, bufferInfo: MediaCodec.BufferInfo) {
        var offset = 4
        //处理00 00 00 01/00 00 01
        if (outputBuffer.get(2).toInt() == 0x01) {
            //如果第三位是01,说明是00 00 01,也是分隔符
            offset = 3
        }
        //获取到第offset位,& 1f,获取到type
        when (outputBuffer.get(offset).toInt() and 0x1f) {
            NAL_SPS -> {
                //sps帧,并且pps和sps是一起输出的,所以无需判断pps
                spsData = ByteArray(bufferInfo.size)
                //由于是投屏,对端不确定什么时候连接,如果发送的数据没有sps,对端有i帧也是无法解析的,所以缓存sps,后续发送i帧的时候把sps一起发送
                //获取数据,并缓存本地
                outputBuffer.get(spsData)
            }
            NAL_I -> {
                //处理I帧
                val iData = ByteArray(bufferInfo.size)
                outputBuffer.get(iData)
                //复制sps数据
                spsData?.also { spsData ->
                    val byteArray = ByteArray(spsData.size + iData.size)
                    System.arraycopy(spsData, 0, byteArray, 0, spsData.size)
                    System.arraycopy(iData, 0, byteArray, spsData.size, iData.size)
                    sendData(byteArray)
                }
            }
            else -> {
                //其他帧,判断是否有sps,无则不需要发,一开始就有问题
                spsData?.also {
                    val byteArray = ByteArray(bufferInfo.size)
                    outputBuffer.get(byteArray)
                    sendData(byteArray)
                }
            }
        }
    }

    private fun sendData(byteArray: ByteArray) {
        Log.i(TAG, "sendData: ${byteArray.size}")
        push?.sendData(byteArray)
    }

    fun startBroadcast(port: String) {
        push = SocketPush(port).apply { start() }
    }

    private fun computPts(): Long {
        //微秒
        return (1_000_000).toLong() / RATE * index
    }

    fun release() {
        camera?.apply {
            Log.i(TAG, "release: ")
            stopPreview()
            release()
        }
        push?.close()
    }
}