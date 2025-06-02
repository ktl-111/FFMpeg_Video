package com.example.play.utils

import com.example.play.config.OutConfig
import java.nio.ByteBuffer

object DecodeUtils {
    init {
        System.loadLibrary("ffmpegplayer")
    }

    fun startDecode(srcPath: String,
                    destPath: String,
                    startTime: Long,
                    endTime: Long,
                    config: OutConfig?,
                    cb: FFMpegUtils.VideoCuttingInterface) {
        nativeCutting(srcPath, destPath, startTime, endTime,config, cb)
    }

    fun writeData(buffer: ByteBuffer, time: Long): Int {
        return nativeWriteData(buffer, time)
    }


    fun endDecode() {
        nativeEndDecode()
    }

    private external fun nativeWriteData(buffer: ByteBuffer, time: Long): Int
    private external fun nativeEndDecode()
    private external fun nativeCutting(
        srcPath: String,
        destPath: String,
        startTime: Long,
        endTime: Long,
        config: OutConfig?,
        cb: FFMpegUtils.VideoCuttingInterface
    )

}