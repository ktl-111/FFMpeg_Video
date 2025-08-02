package com.example.videolearn.play

import android.view.Surface
import com.example.play.config.OutConfig

sealed class Operate(val outConfig: OutConfig, val textureIndex: Int) {
    fun getOperateSize() = 3

    class PlayOperate(outConfig: OutConfig = OutConfig(), val surface: Surface, val playCallback: PlayCallback) : Operate(outConfig, 0) {
        override fun toString() = "PlayOperate(${hashCode()})"
    }

    class EditingOperate(outConfig: OutConfig = OutConfig(), val destPath: String, val startTime: Long, val allTime: Long, val editingCallback: EditingCallback) : Operate(outConfig, 1) {
        internal var callFirstSeek = false
        internal var cuttingDone = false
        override fun toString() = "EditingOperate(${hashCode()})"
    }

    class TrackOperate(outConfig: OutConfig, val videoTrackCallback: VideoTrackCallback) : Operate(outConfig, 2) {
        override fun toString() = "TrackOperate(${hashCode()})"
    }
}
