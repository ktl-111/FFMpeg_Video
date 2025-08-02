package com.example.videolearn.play

import com.example.play.Step

interface VideoControlApi {
    fun start()
    fun stop()
    fun translation(scale: Float, translationX: Float, translationY: Float)
}

interface PlaybackControlApi : VideoControlApi {
    fun resume()
    fun pause()
}

interface VideoPlaybackApi : PlaybackControlApi {
    fun seek(time: Long, nextStep: Step = Step.PauseStep)
    fun getCurrTimestamp(): Long
}

interface TrackControlApi : PlaybackControlApi

interface VideoEditingApi : VideoControlApi