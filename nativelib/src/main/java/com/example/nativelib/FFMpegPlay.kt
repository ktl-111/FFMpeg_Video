package com.example.nativelib

import android.util.Log
import android.view.Surface
import android.view.SurfaceView
import java.nio.ByteBuffer
import java.nio.ByteOrder

class FFMpegPlay(private val surfaceView: SurfaceView) {
    private val TAG = "FFMpegPlayer"

    external fun play(url: String, surface: Surface): Boolean
    external fun cutting(destPath: String)
    external fun release()
    external fun resume()
    external fun pause()
    external fun seekTo(duration: Float)
    external fun getVideoFrames(
        width: Int,
        height: Int,
        precise: Boolean,
        callback: VideoFrameCallback
    )

    private fun allocateFrame(width: Int, height: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN)
    }

    interface VideoFrameCallback {
        fun onStart(duration: Double): DoubleArray
        fun onProgress(
            frame: ByteBuffer,
            pts: Double,
            width: Int,
            height: Int,
            rotate: Int,
            index: Int
        ): Boolean

        fun onEnd()
    }

    init {
        System.loadLibrary("nativelib")
    }

    fun onConfig(witdh: Int, height: Int, duration: Long, fps: Int) {
        configCallback?.invoke(duration, fps)
        val ratio = witdh.toFloat() / height
        Log.i(
            TAG, "onConfig: video width:$witdh,height$height ratio:$ratio\n" +
                    "view width:${surfaceView.measuredWidth},height${surfaceView.measuredHeight}"
        )
        val videoHeight: Int
        val videoWidth: Int
        if (height > witdh) {
            videoHeight = surfaceView.measuredHeight
            videoWidth = (videoHeight * ratio).toInt()
        } else {
            videoWidth = surfaceView.measuredWidth
            videoHeight = (videoWidth / ratio).toInt()
        }
        surfaceView.layoutParams.also {
            it.width = videoWidth
            it.height = videoHeight
        }
        surfaceView.requestLayout()
        Log.i(
            TAG,
            "onConfig: change width:${surfaceView.measuredWidth},height:${surfaceView.measuredHeight}\n" +
                    "width:${videoWidth},height:${videoHeight}"
        )
    }

    private fun onCallData(byteArray: ByteArray) {
        Log.i(TAG, "onCallData: ${byteArray.size}")
        dataCallback?.invoke(byteArray)
    }

    private fun onCallCurrTime(currTime: Float) {
        Log.i(TAG, "onCallCurrTime: $currTime")
        playTimeCallback?.invoke(currTime)
    }

    var configCallback: ((Long, Int) -> Unit)? = null
    var dataCallback: ((ByteArray) -> Unit)? = null
    var playTimeCallback: ((Float) -> Unit)? = null
}