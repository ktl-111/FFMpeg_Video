package com.example.play.proxy

import android.view.Surface
import com.example.play.IPaly
import com.example.play.IPalyListener
import com.example.play.PlayerState
import com.example.play.Step
import com.example.play.config.OutConfig
import com.example.play.utils.FFMpegUtils
import com.example.play.utils.LogHelper

internal class FFMpegProxy : IPaly {
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

    override fun prepare(path: String, surface: Surface, outConfig: OutConfig?) {
        if (path.isEmpty()) {
            LogHelper.e(TAG, "prepare path is empty")
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
        nativeRelease(nativeManager)
        palyListener = null
    }

    override fun resume() {
        nativeResume(nativeManager)
    }

    override fun pause() {
        nativePause(nativeManager)
    }

    override fun seekTo(seekTime: Long, nextStep: Step) {
        nativeSeekTo(nativeManager, seekTime)
    }

    override fun surfaceReCreate(surface: Surface) {
        nativeSurfaceReCreate(nativeManager, surface)
    }

    override fun surfaceDestroy() {
        nativeSurfaceDestroy(nativeManager)
    }

    override fun getPlayerState(): PlayerState {
        return PlayerState.fromState(getPlayerState(nativeManager))
    }

    override fun getCurrTimestamp(): Long {
        return getCurrTimestamp(nativeManager)
    }

    override fun cutting(srcPath: String, destPath: String, startTime: Long, endTime: Long, outConfig: OutConfig?, cb: FFMpegUtils.VideoCuttingInterface) {
        nativeCutting(nativeManager, srcPath, destPath, startTime, endTime, outConfig, cb)
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
    private external fun getCurrTimestamp(nativeManager: Long): Long
    private external fun getPlayerState(nativeManager: Long): Int

    private external fun nativeCutting(
        nativeManager: Long,
        srcPath: String,
        destPath: String,
        startTime: Long,
        endTime: Long,
        outConfig: OutConfig?,
        cb: FFMpegUtils.VideoCuttingInterface
    )

    private fun onNativeVideoConfig(width: Int, height: Int, duration: Double, fps: Double, codecName: String) {
        LogHelper.i(
            TAG,
            "onNativeVideoConfig: ${width}*${height} duration:${duration} fps:${fps} codecName:${codecName}"
        )
        palyListener?.onVideoConfig(width, height, duration, fps)
    }

    private fun onNativePalyProgress(time: Double) {
        LogHelper.d(TAG, "onNativePalyProgress: ${time}")
        palyListener?.onPalyProgress(time)
    }

    private fun onNativePalyComplete() {
        LogHelper.i(TAG, "onNativePalyComplete: ")
        palyListener?.onPalyComplete()
    }

    private fun onPlayError(code: Int) {
        LogHelper.e(TAG, "onPlayError code:${code}")
        palyListener?.onPlayError(code)
    }
}