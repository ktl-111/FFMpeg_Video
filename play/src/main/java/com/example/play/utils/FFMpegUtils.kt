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

    private external fun nativeSetNativeLogLevel(logLevel: Int)

    /**
     * 画幅裁切   eg：视频分辨率大小的处理，切成手表的长宽比例
     *
     * @param src    原视频路径
     * @param dest   输出视频路径
     * @param width  分辨率调整后的宽
     * @param height 分辨率调整后的高
     * @return 状态 0成功
     */
    external fun crop(src: String?, dest: String?, width: Int, height: Int): Int

    private fun allocateFrame(width: Int, height: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN)
    }

    /**
     *设置nativelog打印等级,生产环境建议只打印error
     * @param logLevel Int [android.util.Log]
     */
    fun setNativeLogLevel(logLevel: Int) {
        LogHelper.i("utils", "setNativeLogLevel ${logLevel}")
        nativeSetNativeLogLevel(logLevel)
    }

    /**
     * 添加sdk层logproxy
     * @param logProxy LogProxy
     */
    fun addLogProxy(logProxy: LogProxy) {
        LogHelper.addLogProxy(logProxy)
    }

    fun removeLogProxy(logProxy: LogProxy) {
        LogHelper.removeLogProxy(logProxy)
    }
}