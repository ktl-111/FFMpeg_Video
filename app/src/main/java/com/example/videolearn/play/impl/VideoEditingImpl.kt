package com.example.videolearn.play.impl

import android.media.MediaFormat
import com.example.play.Step
import com.example.play.utils.DecodeUtils
import com.example.videolearn.play.Operate
import com.example.videolearn.play.VideoEditingApi
import kotlinx.coroutines.launch
import java.nio.ByteBuffer

class VideoEditingImpl(path: String, operate: Operate.EditingOperate, videoFormat: MediaFormat) : VideoManagerImpl<Operate.EditingOperate>(path, operate, videoFormat), VideoEditingApi {
    override fun updateProgressPreCheck(time: Long): Boolean {
        if (operate.cuttingDone) {
            return true
        }
        if (!operate.callFirstSeek) {
            operate.callFirstSeek = true
            mediaScope.launch {
                playManager.seekTo(operate.startTime, Step.PlayStep)
            }
        }
        return false
    }

    override fun updateProgressFrame(buffer: ByteBuffer, width: Int, height: Int, time: Long) {
        val result = DecodeUtils.writeData(buffer, time)
        if (result == 10000) {
            operate.cuttingDone = true
            DecodeUtils.endDecode()
            mediaScope.launch {
                playManager.stop()
            }
        }
    }

}