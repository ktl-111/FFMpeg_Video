package com.example.videolearn.shotscreen.service

import android.content.Context
import android.hardware.display.DisplayManager
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.utils.FileUtils
import com.example.videolearn.utils.ResultUtils
import java.nio.ByteBuffer

class ProjectionH264Push(private val context: Context, private val prot: String) : Runnable {

    private lateinit var mediaCodec: MediaCodec
    private lateinit var mediaProjection: MediaProjection

    private val width = 720
    private val height = 1280
    private val socketPush: SocketPush = SocketPush(prot)

    fun startPush() {

        val mediaProjectionManager =
            context.getSystemService(AppCompatActivity.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
        val createScreenCaptureIntent = mediaProjectionManager.createScreenCaptureIntent()
        ResultUtils.getInstance(context as AppCompatActivity).request(
            createScreenCaptureIntent, 100
        ) { requestCode, resultCode, data ->
            if (requestCode == 100 && resultCode == AppCompatActivity.RESULT_OK) {
                socketPush.start()
                mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, data)
                initMediaConfig()
                Toast.makeText(context, "开始推流", Toast.LENGTH_SHORT).show()
                Thread(this).start()
            }
        }
    }

    private fun initMediaConfig() {
        kotlin.runCatching {

            val createVideoFormat =
                MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
            //帧率
            createVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 30)
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
                outputBuffer?.also {
                    parseData(it, bufferInfo)
                }
                mediaCodec.releaseOutputBuffer(outIndex, false)

            }
        }
    }

    private var spsData: ByteArray? = null
    private val NAL_I = 5
    private val NAL_SPS = 7
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
                    socketPush.sendData(byteArray)
                }
            }
            else -> {
                //其他帧,判断是否有sps,无则不需要发,一开始就有问题
                spsData?.also {
                    val byteArray = ByteArray(bufferInfo.size)
                    outputBuffer.get(byteArray)
                    socketPush.sendData(byteArray)
                }
            }
        }
    }

    fun release() {
        kotlin.runCatching {
            socketPush.close()
            mediaCodec.release()
        }.onFailure {
            it.printStackTrace()
        }
    }
}