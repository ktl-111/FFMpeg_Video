package com.example.play

import android.view.Surface
import com.example.play.config.OutConfig

interface IPaly {
    fun init(iPalyListener: IPalyListener?)
    fun perpare(path: String, surface: Surface, outConfig: OutConfig? = null)
    fun start()
    fun stop()
    fun resume()
    fun pause()
    fun release()
    fun seekTo(seek: Long)
    fun surfaceReCreate(surface: Surface)
    fun surfaceDestroy()
}