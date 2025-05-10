package com.norman.android.hdrsample.player.decode.base

import android.media.MediaExtractor
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import com.example.play.IPalyListener
import com.example.play.PlayManager
import com.norman.android.hdrsample.handler.MessageHandler
import com.norman.android.hdrsample.handler.MessageHandler.LifeCycleCallback
import com.norman.android.hdrsample.opengl.GLTextureSurface
import com.norman.android.hdrsample.player.VideoPlayerImpl
import com.norman.android.hdrsample.player.source.FileSource
import com.norman.android.hdrsample.util.LogUtils
import java.io.IOException
import java.nio.ByteBuffer

//new LocalFileSource("/storage/emulated/0/test/VID20241218191908_HDR.mp4")

class FFmpegDecode(private val mimeType: String, private val fileSource: FileSource) : IPalyListener, DecodecApi {
    private val TAG = "FFmpegDecode"
    private val playManager = PlayManager().also {
        it.init(this)
    }
    private var callback: MediaCodecAsyncAdapter.CallBack? = null
    private var surface: Surface? = null


    private val messageHandler: MessageHandler by lazy {
        MessageHandler.obtain(VideoPlayerImpl.VIDEO_PLAYER_NAME,
            object : LifeCycleCallback {
            })
    }

    @Synchronized
    fun post(runnable: Runnable?) {
        messageHandler.post(runnable)
    }

    @Synchronized
    fun postDelayed(runnable: Runnable?, delay: Long) {
        messageHandler.postDelayed(runnable, delay)
    }


    data class Prepare(var surface: Surface?, var start: Boolean)

    private val prepare: Prepare by lazy {
        Prepare(null, false)
    }

    @Synchronized
    override fun setOutputSurface(surface: Surface?) {
        LogUtils.i(TAG, "setOutputSurface: file:${fileSource.path} ${prepare}")
        this.surface = surface
        prepare.surface = this.surface
        startPlay()
    }

    fun startPlay() {
        post {
            LogUtils.i(TAG, "startPlay ${prepare}")
            if (prepare.surface != null) {
                playManager.prepare(fileSource.path, surface, null)
                if (surface is GLTextureSurface) {
                    (surface as GLTextureSurface).setOnFrameAvailableListener { surface ->
                        LogUtils.i(TAG, "onFrameAvailable $surface")
                        try {
                            callback?.onOutputBufferComplete(0)
                        } catch (e: Exception) {
                            LogUtils.i(TAG, "onOutputBufferRender faile " + e.message)
                            e.printStackTrace()
                        }
                    }
                }
            }
            if (prepare.start) {
                playManager.start()
            }
        }
    }

    @Synchronized
    override fun configure(mediaFormat: MediaFormat?,
                           surfaceMode: Boolean,
                           callback: MediaCodecAsyncAdapter.CallBack) {
        LogUtils.i(TAG, "configure")
        this.callback = callback

    }

    override fun start() {
        prepare.start = true
        LogUtils.i(TAG, "start: ${prepare}")
        startPlay()
    }


    override fun pause() {
        LogUtils.i(TAG, "pause: ")
        playManager.pause()
    }

    override fun resume() {
        LogUtils.i(TAG, "resume: ")
        playManager.resume()
    }

    override fun flush() {
        LogUtils.i(TAG, "flush: ")

    }

    override fun stop() {
        LogUtils.i(TAG, "stop: ")
        playManager.stop()
    }

    override fun reset() {
        LogUtils.i(TAG, "reset: ")

    }

    override fun release() {
        LogUtils.i(TAG, "release: ")
        messageHandler.removeAllMessage()
    }

    override fun onVideoConfig(width: Int, height: Int, duration: Double, fps: Double, rotation: Int) {
        LogUtils.i(TAG, "onVideoConfig ${width}*${height} rotation:${rotation}")
        val mediaFormat = getMediaFormat(fileSource.path)?.also {
            it.setInteger(MediaFormat.KEY_WIDTH, width)
            it.setInteger(MediaFormat.KEY_HEIGHT, height)
            it.setInteger(MediaFormat.KEY_ROTATION, rotation)
        }
        LogUtils.i(TAG, "onVideoConfig: ${mediaFormat}")
        callback?.onOutputFormatChanged(mediaFormat) ?: kotlin.run {
            LogUtils.i(TAG, "not call format change")
        }
    }

    private fun getMediaFormat(filePath: String): MediaFormat? {

        val mediaExtractor = MediaExtractor()
        try {
            // 设置数据源
            mediaExtractor.setDataSource(filePath)

            // 获取轨道数量
            val trackCount = mediaExtractor.trackCount
            Log.d(TAG, "Track count: $trackCount")
            for (i in 0 until trackCount) {
                // 获取每个轨道的 MediaFormat
                val format = mediaExtractor.getTrackFormat(i)
                val mimeType = format.getString(MediaFormat.KEY_MIME)
                Log.d(TAG, "Track $i MediaFormat: $format")
                Log.d(TAG, "MIME type: $mimeType")

                // 如果需要特定类型的轨道，可以在这里进行判断
                if (mimeType!!.startsWith("video/")) {
                    Log.d(TAG, "Video track found: $format")
                    return format
                }
            }
        } catch (e: IOException) {
            Log.e(TAG, "Failed to set data source", e)
        } finally {
            mediaExtractor.release()
        }
        return null
    }

    override fun onPalyProgress(frame: ByteBuffer?, time: Double) {
        LogUtils.i(TAG, "onPalyProgress: ${frame?.hashCode()} time:${time}")
//        callback?.onOutputBufferAvailable(frame, 0L)
//        kotlin.runCatching {
//            callback?.onOutputBufferComplete(0)
//        }.onFailure {
//            LogUtils.i(TAG, "onOutputBufferComplete faile ${it.message}")
//            it.printStackTrace()
//        }
    }

    override fun onPalyComplete() {
        LogUtils.i(TAG, "onPalyComplete: ")
    }

    override fun onPlayError(code: Int) {
    }


    override fun isSupport10BitYUV420() = true
    override fun getCodecName(): String {
        return "OMX.qcom.video.decoder.hevc"
    }

    override fun isSupportColorFormat(colorFormat: Int) = true
}