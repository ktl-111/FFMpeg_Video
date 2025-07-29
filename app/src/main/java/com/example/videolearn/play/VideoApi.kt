package com.example.videolearn.play

import com.example.play.Step

interface BaseVideoApi {
    fun stop()
    fun resume()
    fun pause()
    fun release()
}

interface PlayVideoApi : BaseVideoApi {
    fun start()
    fun seek(time: Long, nextStep: Step = Step.PauseStep)
}

interface TrackApi : BaseVideoApi {
    fun trackStart()
}

interface CuttingApi {
    fun cuttingStart()
}