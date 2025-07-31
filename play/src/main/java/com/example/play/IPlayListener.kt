package com.example.play

import java.nio.ByteBuffer

interface IPlayListener {
    fun onVideoConfig(witdh: Int, height: Int, duration: Double, fps: Double,rotation:Int)

    /**
     * On play progress
     *
     * @param time ms
     */
    fun onPlayProgress(frame: ByteBuffer?, time: Double)
    fun onPlayComplete()
    fun onPlayError(code: Int)
}