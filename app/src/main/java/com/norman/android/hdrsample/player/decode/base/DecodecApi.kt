package com.norman.android.hdrsample.player.decode.base

import android.media.MediaFormat
import android.view.Surface

interface DecodecApi {

    fun setOutputSurface(surface: Surface?)

    fun configure(mediaFormat: MediaFormat?,
                  surfaceMode: Boolean,
                  callback: MediaCodecAsyncAdapter.CallBack)

    fun start()
    fun pause()
    fun resume()
    fun flush()
    fun stop()
    fun reset()
    fun release()
    fun isSupportColorFormat(colorFormat: Int): Boolean
    fun isSupport10BitYUV420(): Boolean
    fun getCodecName(): String
}