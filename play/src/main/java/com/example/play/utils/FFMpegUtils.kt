package com.example.play.utils

import androidx.annotation.Keep
import com.example.play.config.OutConfig
import java.nio.ByteBuffer
import java.nio.ByteOrder

@Keep
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
         * @return Boolean 是否结束
         */
        fun onProgress(
            frame: ByteBuffer, timestamps: Double, width: Int, height: Int, rotate: Int, index: Int
        ): Boolean

        /**
         * 抽帧动作结束
         */
        fun onEnd()
    }

    interface VideoCuttingInterface {
        fun onStart()

        /**
         *
         * @param progress Double 0-100
         */
        fun onProgress(progress: Double)

        /**
         *
         * @param resultCode Int globals.h
         */
        fun onFail(resultCode: Int)
        fun onDone()
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
     * 裁剪
     * @param srcPath String
     * @param destPath String
     * @param startTime Long ms
     * @param endTime Long ms
     * @param outConfig OutConfig?
     * @param cb VideoCuttingInterface
     */
    fun cutting(
        srcPath: String,
        destPath: String,
        startTime: Long,
        endTime: Long,
        outConfig: OutConfig?,
        cb: VideoCuttingInterface
    ) {
        nativeCutting(srcPath, destPath, startTime, endTime, outConfig, cb)
    }

    private external fun nativeCutting(
        srcPath: String,
        destPath: String,
        startTime: Long,
        endTime: Long,
        outConfig: OutConfig?,
        cb: VideoCuttingInterface
    )

    private fun allocateFrame(width: Int, height: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN)
    }

}