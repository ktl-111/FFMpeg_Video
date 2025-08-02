package com.example.videolearn.play.impl

import android.media.MediaFormat
import com.example.videolearn.play.Operate
import com.example.videolearn.play.TrackControlApi
import java.nio.ByteBuffer

class TrackControlImpl(path: String, operate: Operate.TrackOperate, videoFormat: MediaFormat) : VideoManagerImpl<Operate.TrackOperate>(path, operate, videoFormat), TrackControlApi {
    private var preTime = -1
    override fun updateProgressPreCheck(time: Long): Boolean {
        val currTime = (time / 1000).toInt()
        if (preTime == currTime) {
            return true
        }
        preTime = currTime
        return false
    }

    override fun updateProgressFrame(buffer: ByteBuffer, width: Int, height: Int, time: Long) {
        operate.videoTrackCallback.onVideoTrackUpdate(buffer, width, height, time)
    }
}