package com.example.play.utils

import android.os.Build
import androidx.annotation.Keep
import com.example.play.config.OutConfig
import com.example.play.data.DecodeData
import com.example.play.data.DecodeFrameData
import com.example.play.data.PhoneData
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

    /**
     * 获取解码数据
     * @param path String?
     * @param useHwDecode Boolean 是否硬解
     * @return DecodeData
     */
    fun getDecodeData(path: String?, useHwDecode: Boolean): DecodeData {
        return if (path.isNullOrEmpty()) {
            DecodeData(prepareMsg = "path is null or empty")
        } else {
            nativeTestDecode(path, useHwDecode, DecodeData())
        }.also {
            it.useHwDecode = useHwDecode
            it.phoneData = PhoneData(Build.MODEL, isCpuV8(), Build.VERSION.RELEASE, Runtime.getRuntime().availableProcessors())
        }
    }

    private fun isCpuV8(): Boolean {
        return kotlin.runCatching {
            Build.SUPPORTED_64_BIT_ABIS.find { it == "arm64-v8a" } != null
        }.getOrElse { false }
    }

    private external fun nativeTestDecode(path: String, useHwDecode: Boolean, decodeData: DecodeData): DecodeData

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

    private fun allocateFrame(width: Int, height: Int): ByteBuffer {
        return ByteBuffer.allocateDirect(width * height * 4).order(ByteOrder.LITTLE_ENDIAN)
    }

    private fun allocateDecodeFrameData(): DecodeFrameData = DecodeFrameData()

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