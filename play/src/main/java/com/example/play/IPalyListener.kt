package com.example.play

import java.nio.ByteBuffer

interface IPalyListener {
    fun onVideoConfig(witdh: Int, height: Int, duration: Double, fps: Double,rotation:Int)

    /**
     * On paly progress
     *
     * @param time ms
     */
    fun onPalyProgress(frame: ByteBuffer?, time: Double)
    fun onPalyComplete()
    fun onPlayError(code: Int)
}