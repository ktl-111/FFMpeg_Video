package com.example.play.proxy

import android.util.Log
import android.view.Surface
import com.example.play.IPaly
import com.example.play.IPalyListener
import com.example.play.config.OutConfig

class FFMpegProxy : IPaly {
    init {
        System.loadLibrary("ffmpegplayer")
    }

    private var outConfig: OutConfig? = null
    private val TAG = "FFMpegProxy"
    private var nativeManager: Long = -1
    private var palyListener: IPalyListener? = null
    override fun init(iPalyListener: IPalyListener?) {
        palyListener = iPalyListener
        nativeManager = nativeInit()
    }

    override fun perpare(path: String, surface: Surface, outConfig: OutConfig?) {
        if (path.isEmpty()) {
            Log.e(TAG, "perpare path is empty")
            return
        }
        this.outConfig = outConfig;
        nativePrepare(nativeManager, path, surface, outConfig)
    }

    override fun start() {
        nativeStart(nativeManager)
    }

    override fun stop() {
        nativeStop(nativeManager)
    }

    override fun resume() {
        nativeResume(nativeManager)
    }

    override fun pause() {
        nativePause(nativeManager)
    }

    override fun release() {
        nativeRelease(nativeManager)
        palyListener = null
    }

    override fun seekTo(seekTime: Long) {
        nativeSeekTo(nativeManager, seekTime)
    }

    override fun surfaceReCreate(surface: Surface) {
        nativeSurfaceReCreate(nativeManager,surface)
    }

    override fun surfaceDestroy() {
        nativeSurfaceDestroy(nativeManager)
    }
    private external fun nativeInit(): Long
    private external fun nativePrepare(
        nativeManager: Long, path: String, surface: Surface, outConfig: OutConfig?
    ): Boolean

    private external fun nativeStart(nativeManager: Long)
    private external fun nativeStop(nativeManager: Long)
    private external fun nativeResume(nativeManager: Long)
    private external fun nativePause(nativeManager: Long)
    private external fun nativeRelease(nativeManager: Long)
    private external fun nativeSeekTo(nativeManager: Long, seekTime: Long): Boolean
    private external fun nativeSurfaceReCreate(nativeManager: Long, surface: Surface)
    private external fun nativeSurfaceDestroy(nativeManager: Long)
    private fun onNativeVideoConfig(width: Int, height: Int, duration: Double, fps: Double) {
        Log.i(
            TAG,
            "onNativeVideoConfig: ${width}*${height} duration:${duration} fps:${fps} palyListener:${palyListener}"
        )
        palyListener?.onVideoConfig(width, height, duration, fps)
    }

    private fun onNativePalyProgress(time: Double) {
        Log.i(TAG, "onNativePalyProgress: ${time}")
        palyListener?.onPalyProgress(time)
    }

    private fun onNativePalyComplete() {
        Log.i(TAG, "onNativePalyComplete: ")
        palyListener?.onPalyComplete()
    }
}