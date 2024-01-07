package com.example.play

import android.view.Surface
import com.example.play.PlayerState
import com.example.play.config.OutConfig

interface IPaly {
    fun init(iPalyListener: IPalyListener?)
    fun prepare(path: String, surface: Surface, outConfig: OutConfig? = null)
    fun start()
    fun stop()
    fun resume()
    fun pause()
    fun release()

    /**
     *
     * @param seek Long ms
     */
    fun seekTo(seek: Long)
    fun surfaceReCreate(surface: Surface)
    fun surfaceDestroy()
    fun getPlayerState(): PlayerState

    /**
     *
     * @return Long ms
     */
    fun getCurrTimestamp(): Long
}