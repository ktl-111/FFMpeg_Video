package com.example.videolearn.play

import com.example.play.Step

interface VideoControlApi {
    fun start()
    fun stop()
}

interface PlaybackControlApi : VideoControlApi {
    fun resume()
    fun pause()
}

interface VideoPlaybackApi : PlaybackControlApi {
    fun seek(time: Long, nextStep: Step = Step.PauseStep)
}

interface TrackControlApi : PlaybackControlApi

interface VideoEditingApi : VideoControlApi