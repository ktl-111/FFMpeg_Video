package com.example.videolearn.test

import android.content.Context
import android.hardware.display.DisplayManager
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.utils.FileUtils
import com.example.videolearn.utils.ResultUtils

class ProjectionH264Encoder(private val context: Context) : Runnable {

    private lateinit var mediaCodec: MediaCodec
    private lateinit var mediaProjection: MediaProjection

    private val width = 720
    private val height = 1080
    fun startProjection() {
        val mediaProjectionManager =
            context.getSystemService(AppCompatActivity.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
        val createScreenCaptureIntent = mediaProjectionManager.createScreenCaptureIntent()
        ResultUtils.getInstance(context as AppCompatActivity).request(
            createScreenCaptureIntent, 100
        ) { requestCode, resultCode, data ->
            if (requestCode == 100 && resultCode == AppCompatActivity.RESULT_OK) {
                mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, data)
                initMediaConfig()

                Thread(this).start()
            }
        }
    }

    private fun initMediaConfig() {
        kotlin.runCatching {

            val createVideoFormat =
                MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
            //帧率
            createVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 20)
            //多少帧一个I，非强制性
            createVideoFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 30)
            //码率,越大越清晰
            createVideoFormat.setInteger(MediaFormat.KEY_BIT_RATE, width * height)
            //来源
            createVideoFormat.setInteger(
                MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
            )

            mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            //设置参数
            mediaCodec.configure(
                createVideoFormat,
                null,
                null,
                MediaCodec.CONFIGURE_FLAG_ENCODE
            )
        }.onFailure {
            it.printStackTrace()
        }
    }

    override fun run() {
        //创建数据存放处
        val createInputSurface = mediaCodec.createInputSurface()
        //和surface绑定
        mediaProjection.createVirtualDisplay(
            "test",/*id,唯一*/
            width,
            height,
            1,
            DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
            createInputSurface,
            null,
            null
        )
        mediaCodec.start()
        val bufferInfo = MediaCodec.BufferInfo()
        while (true) {
            //获取已编码的buffer index
            val outIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
            if (outIndex >= 0) {
                //拿到对应buffer
                val outputBuffer = mediaCodec.getOutputBuffer(outIndex)
                val byteArray = ByteArray(bufferInfo.size)
                //读取buffer
                outputBuffer?.get(byteArray)
                val name = "projection"
                FileUtils.writeBytes(byteArray, name)
                FileUtils.writeContent(byteArray, name)
                mediaCodec.releaseOutputBuffer(outIndex, false)
            }
        }
    }

}