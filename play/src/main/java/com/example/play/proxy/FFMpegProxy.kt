package com.example.play.proxy

import android.view.Surface
import com.example.play.IPlay
import com.example.play.IPlayListener
import com.example.play.PlayerState
import com.example.play.Step
import com.example.play.TrackInterceptor
import com.example.play.config.OutConfig
import com.example.play.utils.FFMpegUtils
import com.example.play.utils.LogHelper
import java.nio.ByteBuffer
import java.nio.ByteOrder

internal class FFMpegProxy : IPlay {
    init {
        System.loadLibrary("ffmpegplayer")
    }

    private var outConfig: OutConfig? = null
    private val TAG = "FFMpegProxy"
    private var nativeManager: Long = -1
    private var palyListener: IPlayListener? = null
    private var trackInterceptor: TrackInterceptor? = null
    override fun init(iPlayListener: IPlayListener?) {
        palyListener = iPlayListener
        nativeManager = nativeInit()
    }

    override fun setTrackInterceptor(interceptor: TrackInterceptor) {
        this.trackInterceptor = interceptor
    }

    override fun prepare(path: String, surface: Surface?, outConfig: OutConfig?) {
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
        nativeManager: Long, path: String, surface: Surface?, outConfig: OutConfig?
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

    private fun onAllocateFrame(size: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(size).order(ByteOrder.LITTLE_ENDIAN)
    }

    private fun onNativeVideoConfig(width: Int, height: Int, duration: Double, fps: Double, rotation: Int, codecName: String) {
        LogHelper.i(
            TAG,
            "onNativeVideoConfig: ${width}*${height} duration:${duration} fps:${fps} rotation:${rotation} codecName:${codecName}"
        )
        palyListener?.onVideoConfig(width, height, duration, fps, rotation)
    }

    private fun onNativePlayProgress(frame: ByteBuffer?, time: Double) {
        LogHelper.d(TAG, "onNativePlayProgress: ${time}")
        palyListener?.onPlayProgress(frame, time)
    }

    private fun onNativePlayComplete() {
        LogHelper.i(TAG, "onNativePlayComplete: ")
        palyListener?.onPlayComplete()
    }

    private fun onPlayError(code: Int) {
        LogHelper.e(TAG, "onPlayError code:${code}")
        palyListener?.onPlayError(code)
    }

    private fun onNativeTrackInterceptor(duration: Double): LongArray? {
        return trackInterceptor?.onStart(duration)
    }
}