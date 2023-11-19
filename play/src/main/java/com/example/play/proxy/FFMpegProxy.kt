package com.example.play.proxy

import android.util.Log
import android.view.Surface
import com.example.play.IPaly
import com.example.play.IPalyListener

class FFMpegProxy : IPaly {
    init {
        System.loadLibrary("ffmpegplayer")
    }

    private val TAG = "FFMpegProxy"
    private var nativeManager: Long = -1
    private var palyListener: IPalyListener? = null
    override fun init(iPalyListener: IPalyListener?) {
        palyListener = iPalyListener
        nativeManager = nativeInit()
    }

    override fun perpare(path: String, surface: Surface) {
        if (path.isEmpty()) {
            Log.e(TAG, "perpare path is empty")
            return
        }
        nativePrepare(nativeManager, path, surface)
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
    }

    private external fun nativeTest(): Long
    private external fun nativeInit(): Long
    private external fun nativePrepare(nativeManager: Long, path: String, surface: Surface): Boolean
    private external fun nativeStart(nativeManager: Long)
    private external fun nativeStop(nativeManager: Long)
    private external fun nativeResume(nativeManager: Long)
    private external fun nativePause(nativeManager: Long)
    private external fun nativeRelease(nativeManager: Long)

    private fun onNativeVideoConfig(width: Int, height: Int, duration: Long, fps: Int) {
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