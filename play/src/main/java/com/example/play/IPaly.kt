package com.example.play

import android.view.Surface

interface IPaly {
    fun init(iPalyListener: IPalyListener?)
    fun perpare(path: String, surface: Surface)
    fun start()
    fun stop()
    fun resume()
    fun pause()
    fun release()
}