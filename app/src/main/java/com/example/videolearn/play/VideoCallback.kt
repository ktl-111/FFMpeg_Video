package com.example.videolearn.play

import java.nio.ByteBuffer


interface PlayCallback {
    fun onPlayProgress(time: Long)
    fun onPlayComplete()
}

interface VideoTrackCallback {
    fun onVideoTrackUpdate(byteBuffer: ByteBuffer, width: Int, height: Int, time: Long)
    fun videoTrackInterval(videoDuration: Long): LongArray
}

interface EditingCallback {
    fun onStart()
    fun onEditingProgress(progress: Double)
    fun onEditingDone()
    fun onFail(errorCode: Int)
}