package com.example.nativelib

import android.util.Log
import android.view.Surface
import android.view.SurfaceView

class FFMpegPlay(private val surfaceView: SurfaceView) {
    val TAG = "FFMpegPlay"
    external fun play(url: String, surface: Surface): Boolean
    external fun cutting(destPath: String)
    external fun release()
    external fun resume()
    external fun pause()
    external fun seekTo(duration: Float)

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