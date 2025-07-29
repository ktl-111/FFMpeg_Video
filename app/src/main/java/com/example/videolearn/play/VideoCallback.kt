package com.example.videolearn.play

import java.nio.ByteBuffer


interface PlayCallback {
    fun onPlayProgress(time: Long)
    fun onPlayComplete()
}

interface VideoTrackCallback {
    fun onVideoTrackResult(byteBuffer: ByteBuffer, width: Int, height: Int, time: Long)
    fun videoTrackInterval(videoDuration: Long): LongArray
}

interface CuttingCallback {
    fun onStart()
    fun onCuttingProgress(progress: Double)
    fun onCuttingDone()
    fun onFail(errorCode: Int)
}