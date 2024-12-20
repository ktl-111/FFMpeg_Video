package com.example.play

import android.view.Surface
import com.example.play.config.OutConfig
import com.example.play.utils.FFMpegUtils

interface IPaly {
    fun init(iPalyListener: IPalyListener?)
    fun prepare(path: String, surface: Surface, outConfig: OutConfig? = null)
    fun start()
    fun stop()
    fun resume()
    fun pause()

    /**
     *
     * @param seekTime Long ms
     */
    fun seekTo(seekTime: Long, nextStep: Step = Step.UnknownStep)
    fun surfaceReCreate(surface: Surface)
    fun surfaceDestroy()
    fun getPlayerState(): PlayerState

    /**
     *
     * @return Long ms
     */
    fun getCurrTimestamp(): Long

    fun cutting(srcPath: String, destPath: String, startTime: Long, endTime: Long, outConfig: OutConfig?, cb: FFMpegUtils.VideoCuttingInterface)
}