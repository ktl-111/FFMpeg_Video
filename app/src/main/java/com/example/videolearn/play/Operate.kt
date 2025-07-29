package com.example.videolearn.play

import android.view.Surface
import com.example.play.config.OutConfig

sealed class Operate(val outConfig: OutConfig, val textureIndex: Int) {
    fun getOperateSize() = 3

    class PlayOperate(outConfig: OutConfig = OutConfig(), val surface: Surface, val playCallback: PlayCallback) : Operate(outConfig, 0) {
        override fun toString() = "PlayOperate(${hashCode()})"
    }

    class CuttingOperate(outConfig: OutConfig = OutConfig(), val destPath: String, val startTime: Long, val allTime: Long, val cuttingCallback: CuttingCallback) : Operate(outConfig, 1) {
        override fun toString() = "CuttingOperate(${hashCode()})"
    }

    class TrackOperate(outConfig: OutConfig, val trackCallback: VideoTrackCallback) : Operate(outConfig, 2) {
        override fun toString() = "TrackOperate(${hashCode()})"
    }
}
