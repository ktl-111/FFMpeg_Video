package com.example.nativelib

import android.util.Log
import android.view.Surface
import android.view.SurfaceView
import android.view.ViewGroup.LayoutParams

class FFMpegPlay(private val surfaceView: SurfaceView) {
    val TAG = "FFMpegPlay"
    external fun play(url: String, surface: Surface): Boolean
    external fun cutting(destPath: String)
    external fun release()

    init {
        System.loadLibrary("nativelib")
    }

    fun onConfig(witdh: Int, height: Int) {
        val ratio = witdh.toFloat() / height
        val videoWidth = surfaceView.context.resources.displayMetrics.widthPixels
        val videoHeight = (videoWidth / ratio).toInt()
        surfaceView.layoutParams = LayoutParams(videoWidth, videoHeight)
    }

    private fun onCallData(byteArray: ByteArray) {
        Log.i(TAG, "onCallData: ${byteArray.size}")
        callback?.invoke(byteArray)
    }

    var callback: ((ByteArray) -> Unit)? = null
}