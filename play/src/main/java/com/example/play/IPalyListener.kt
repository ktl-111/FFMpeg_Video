package com.example.play

interface IPalyListener {
    fun onVideoConfig(witdh: Int, height: Int, duration: Double, fps: Double)

    /**
     * On paly progress
     *
     * @param time ms
     */
    fun onPalyProgress(time: Double)
    fun onPalyComplete()
    fun onPlayError(code: Int)
}