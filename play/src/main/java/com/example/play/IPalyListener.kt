package com.example.play

interface IPalyListener {
    fun onVideoConfig(witdh: Int, height: Int, duration: Long, fps: Int)

    /**
     * On paly progress
     *
     * @param time ms
     */
    fun onPalyProgress(time: Double)
    fun onPalyComplete()
}