package com.example.nativelib

import android.view.Surface
import android.view.SurfaceView
import android.view.ViewGroup.LayoutParams

class FFMpegPlay(val surfaceView: SurfaceView) {

    external fun play(url: String, surface: Surface): Boolean
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
}