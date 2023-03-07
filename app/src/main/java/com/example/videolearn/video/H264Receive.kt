package com.example.videolearn.video

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import com.example.videolearn.shotscreen.client.SocketConnect

class H264Receive(
    surface: Surface,
    private val ip: String,
    private val prot: String
) :
    SocketConnect.SocketCallback {
    private val mediaCodec: MediaCodec =
        MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
    private var socketConnect: SocketConnect? = null
    private val TAG = "H264Receive"
    init {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, WIDTH, HEIGHT)
        format.setInteger(MediaFormat.KEY_BIT_RATE, WIDTH * HEIGHT)
        format.setInteger(MediaFormat.KEY_FRAME_RATE, RATE)
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 5) //IDR帧刷新时间
        format.setInteger(
            MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
        )
        mediaCodec.configure(
            format,
            surface,
            null, 0
        )

        mediaCodec.start()
    }

    fun startConnect() {
        socketConnect = SocketConnect(ip, prot, this).apply {
            start()
        }
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