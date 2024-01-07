package com.example.play.utils

import com.example.play.config.OutConfig
import java.nio.ByteBuffer
import java.nio.ByteOrder

object FFMpegUtils {

    init {
        System.loadLibrary("ffmpegplayer")
    }

    interface VideoFrameArrivedInterface {
        /**
         * @param duration
         * 给定视频时长，返回待抽帧的pts arr，单位为s
         */
        fun onStart(duration: Double): DoubleArray

        /**
         * 每抽帧一次回调一次
         */
        fun onProgress(
            frame: ByteBuffer, timestamps: Double, width: Int, height: Int, rotate: Int, index: Int
        ): Boolean

        /**
         * 抽帧动作结束
         */
        fun onEnd()
    }

    fun getVideoFrames(
        path: String?, width: Int, height: Int, precise: Boolean, cb: VideoFrameArrivedInterface
    ) {
        if (path.isNullOrEmpty()) return
        getVideoFramesCore(path, width, height, precise, cb)
    }

    private external fun getVideoFramesCore(
        path: String, width: Int, height: Int, precise: Boolean, cb: VideoFrameArrivedInterface
    )

    /**
     * 内容时长裁剪
     */
    fun cutting(
        srcPath: String,
        destPath: String,
        startTime: Double,
        endTime: Double,
        outConfig: OutConfig? = null
    ): Boolean {
        return nativeCutting(srcPath, destPath, startTime, endTime, outConfig)
    }

    private external fun nativeCutting(
        srcPath: String, destPath: String, startTime: Double, endTime: Double, outConfig: OutConfig?
    ): Boolean

    private fun allocateFrame(width: Int, height: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN)
    }

}