package com.example.videolearn.play.impl

import android.media.MediaFormat
import com.example.play.Step
import com.example.play.utils.DecodeUtils
import com.example.play.utils.FFMpegUtils
import com.example.videolearn.play.Operate
import com.example.videolearn.play.VideoEditingApi
import kotlinx.coroutines.launch
import java.nio.ByteBuffer

class VideoEditingImpl(path: String, operate: Operate.EditingOperate, videoFormat: MediaFormat) : VideoManagerImpl<Operate.EditingOperate>(path, operate, videoFormat), VideoEditingApi {
    override fun start() {
        DecodeUtils.startDecode(path, destPath = operate.destPath, startTime = operate.startTime, endTime = operate.startTime + operate.allTime, config = operate.outConfig, object : FFMpegUtils.VideoCuttingInterface {
            override fun onStart() {
                operate.editingCallback.onStart()
            }

            override fun onProgress(progress: Double) {
                operate.editingCallback.onEditingProgress(progress)
            }

            override fun onFail(resultCode: Int) {
                operate.editingCallback.onFail(resultCode)
            }

            override fun onDone() {
                operate.editingCallback.onEditingDone()
            }
        })
        super.start()
    }
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