package com.example.videolearn.play.impl

import android.media.MediaFormat
import com.example.play.Step
import com.example.videolearn.play.Operate
import com.example.videolearn.play.VideoPlaybackApi
import com.norman.android.hdrsample.util.LogUtils

class VideoPlayBackImpl(path: String, operate: Operate.PlayOperate, videoFormat: MediaFormat) : VideoManagerImpl<Operate.PlayOperate>(path, operate, videoFormat), VideoPlaybackApi {
    override fun seek(time: Long, nextStep: Step) {
        LogUtils.i(TAG, "seek ${operate},time:${time},nextSetp:${nextStep}")
        playManager.seekTo(time, nextStep)
    }

    override fun getCurrTimestamp(): Long = playManager.getCurrTimestamp()

}