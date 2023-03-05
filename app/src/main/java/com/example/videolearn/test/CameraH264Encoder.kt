package com.example.videolearn.test

import android.content.Context
import android.hardware.Camera
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.view.SurfaceView
import com.example.videolearn.utils.FileUtils

class CameraH264Encoder(private val context: Context, private val surfaceView: SurfaceView) {
    private lateinit var mediaCodec: MediaCodec
    private var index = 0
    private val RATE = 20
    fun startCamera() {
        val camera = Camera.open(Camera.CameraInfo.CAMERA_FACING_BACK)
        val parameters = camera.parameters
        val previewSize = parameters.previewSize
        //设置画布
        camera.setPreviewDisplay(surfaceView.holder)
        //预览旋转90度
        camera.setDisplayOrientation(90)
        //420
        val byteArray = ByteArray(previewSize.width * previewSize.height * 3 / 2)
        camera.addCallbackBuffer(byteArray)
        camera.setPreviewCallbackWithBuffer { data, camera ->
            //数据未旋转
            encoder(data)
        }

        val createVideoFormat =
            MediaFormat.createVideoFormat(
                MediaFormat.MIMETYPE_VIDEO_AVC,
                previewSize.width,
                previewSize.height
            )
        //帧率
        createVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, RATE)
        //多少帧一个I，非强制性
        createVideoFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 30)
        //码率,越大越清晰
        createVideoFormat.setInteger(
            MediaFormat.KEY_BIT_RATE,
            previewSize.width * previewSize.height
        )
        //来源
        createVideoFormat.setInteger(
            MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
        )

        mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
        //设置参数
        mediaCodec.configure(
            createVideoFormat,
            null,
            null,
            MediaCodec.CONFIGURE_FLAG_ENCODE
        )

        camera.startPreview()
        mediaCodec.start()
    }

    private fun encoder(data: ByteArray) {
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
                    it.put(data)
                }
                //通知dsp开始解码
                mediaCodec.queueInputBuffer(inputIndex, 0, data.size, computPts(), 0)
                index++
            }

            /*============输出===============*/
            val bufferInfo = MediaCodec.BufferInfo()
            //获取已编码的buffer index
            val outputIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
            if (outputIndex >= 0) {
                //拿到对应buffer
                val outputBuffer = mediaCodec.getOutputBuffer(outputIndex)
                val byteArray = ByteArray(bufferInfo.size)
                //读取buffer
                outputBuffer?.get(byteArray)
                val name = "camera"
                FileUtils.writeBytes(byteArray, name)
                FileUtils.writeContent(byteArray, name)
                mediaCodec.releaseOutputBuffer(outputIndex, false)
            }
        }.onFailure {
            it.printStackTrace()
        }

    }

    private fun computPts(): Long {
        //微秒
        return (1_000_000).toLong() / RATE * index
    }
}