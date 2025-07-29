package com.example.play

interface TrackInterceptor {
    fun onStart(duration: Double): LongArray
}