package com.example.videolearn.shotscreen.client

import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import android.widget.Toast
import com.example.videolearn.App
import com.example.videolearn.utils.FileUtils

private const val TAG ="ProjectionH264Connect"
class ProjectionH264Connect(
    surface: Surface,
    private val ip: String,
    private val prot: String
) :
    SocketConnect.SocketCallback {
    private val mediaCodec: MediaCodec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
    var socketConnect: SocketConnect? = null

    init {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 720, 1280)
        format.setInteger(MediaFormat.KEY_BIT_RATE, 720 * 1280)
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 30)
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
        mediaCodec.configure(
            format,
            surface,
            null, 0
        )

        mediaCodec.start()
    }

    fun start() {
        socketConnect = SocketConnect(ip, prot, this).apply {
            start()
        }
        Toast.makeText(App.application, "开始接收", Toast.LENGTH_SHORT).show()
    }

    override fun callBack(data: ByteArray) {
        kotlin.runCatching {

            Log.i(TAG, "callBack: ${data.size}")
            val dequeueInputBuffer = mediaCodec.dequeueInputBuffer(10_000)
            if (dequeueInputBuffer >= 0) {
                val inputBuffer = mediaCodec.getInputBuffer(dequeueInputBuffer)
                inputBuffer?.also {
                    it.clear()
                    it.put(data, 0, data.size)
                }
                //投屏pts为当前时间戳，
                mediaCodec.queueInputBuffer(
                    dequeueInputBuffer,
                    0,
                    data.size,
                    System.currentTimeMillis(),
                    0
                )
            }
            FileUtils.writeContent(data,"CONNECT")
            val bufferInfo = MediaCodec.BufferInfo()
            var dequeueOutputBuffer = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
            //阻塞回调,一直等待解码完成
            while (dequeueOutputBuffer >= 0) {
                mediaCodec.releaseOutputBuffer(dequeueOutputBuffer, true)
                dequeueOutputBuffer = mediaCodec.dequeueOutputBuffer(bufferInfo, 0)
            }
        }.onFailure {
            it.printStackTrace()
        }

    }

    fun release() {
        socketConnect?.stop()
        mediaCodec.release()
    }
}