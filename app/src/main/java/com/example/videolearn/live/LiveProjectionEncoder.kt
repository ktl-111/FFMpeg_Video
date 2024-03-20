package com.example.videolearn.live

import android.content.Context
import android.hardware.display.DisplayManager
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.example.play.utils.MediaScope
import com.example.videolearn.utils.FileUtils
import com.example.videolearn.utils.ResultUtils
import com.example.videolearn.utils.ScreenUtils
import kotlinx.coroutines.launch


class LiveProjectionEncoder(private val context: Context) {
    private val TAG = "LiveProjectionEncoder"
    private lateinit var mediaCodec: MediaCodec
    private lateinit var mediaProjection: MediaProjection

    private val width = ScreenUtils.getScreenWidth(context)
    private val height = ScreenUtils.getScreenHeight(context)

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

                startLoopParseData()
            }
        }
    }


    private fun initMediaConfig() {
        kotlin.runCatching {

            val createVideoFormat =
                MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
            //帧率
            createVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 15)
            //多少帧一个I，非强制性
            createVideoFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
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


    private fun startLoopParseData() {
        MediaScope.launch {
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
            var timeStamp: Long = 0
            var startTime: Long = 0
            while (true) {
                if (System.currentTimeMillis() - timeStamp >= 2000) {
                    val params = Bundle()
                    //立即刷新 让下一帧是关键帧
                    params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0)
                    mediaCodec.setParameters(params)
                    timeStamp = System.currentTimeMillis()
                }
                //获取已编码的buffer index
                var outIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
                while (outIndex >= 0) {
                    if (startTime == 0L) {
                        startTime = bufferInfo.presentationTimeUs / 1000
                    }
                    //拿到对应buffer
                    val outputBuffer = mediaCodec.getOutputBuffer(outIndex)
                    val byteArray = ByteArray(bufferInfo.size)
                    //读取buffer
                    outputBuffer?.get(byteArray)
                    val name = TAG
                    FileUtils.writeContent(byteArray, name)

                    val rtmpPackage = RTMPPackage(
                        byteArray,
                        bufferInfo.presentationTimeUs / 1000 - startTime,
                        RTMP_PACKET_TYPE_VIDEO
                    )

                    ConsumerLive.addPackage(rtmpPackage)
                    mediaCodec.releaseOutputBuffer(outIndex, false)
                    outIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 1_000)
                }
            }
        }
    }
}