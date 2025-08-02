package com.example.videolearn.play

import android.media.MediaExtractor
import android.media.MediaFormat
import android.util.Log
import com.example.videolearn.play.impl.TrackControlImpl
import com.example.videolearn.play.impl.VideoEditingImpl
import com.example.videolearn.play.impl.VideoPlayBackImpl
import com.norman.android.hdrsample.player.color.ColorSpace
import com.norman.android.hdrsample.util.LogUtils
import com.norman.android.hdrsample.util.MediaFormatUtil
import java.io.IOException

class VideoManager(private val path: String) {
    private val TAG = "VideoManager"
    private val _videoFormat by lazy { initVideoFormat() }
    val videoFormat: MediaFormat?
        get() = _videoFormat

    fun prepare(): Boolean {
        return videoFormat != null
    }

    private fun initVideoFormat(): MediaFormat? {
        val mediaExtractor = MediaExtractor()
        try {
            // 设置数据源
            mediaExtractor.setDataSource(path)

            // 获取轨道数量
            val trackCount = mediaExtractor.trackCount
            Log.d(TAG, "Track count: $trackCount")
            for (i in 0 until trackCount) {
                // 获取每个轨道的 MediaFormat
                val format = mediaExtractor.getTrackFormat(i)
                val mimeType = format.getString(MediaFormat.KEY_MIME)
                Log.d(TAG, "Track $i MediaFormat: $format")
                Log.d(TAG, "MIME type: $mimeType")

                // 如果需要特定类型的轨道，可以在这里进行判断
                if (mimeType!!.startsWith("video/")) {
                    Log.d(TAG, "Video track found: $format")
                    MediaFormatUtil.setColorSpace(format, ColorSpace.VIDEO_SDR)
                    return format
                }
            }
        } catch (e: IOException) {
            Log.e(TAG, "Failed to set data source", e)
        } finally {
            mediaExtractor.release()
        }
        return null
    }

    fun getPlayManager(operate: Operate.PlayOperate): VideoPlaybackApi {
        return VideoPlayBackImpl(path = path, operate = operate, videoFormat = videoFormat!!).also {
            LogUtils.i(TAG, "getPlayManager")
        }
    }

    fun getTrackManager(operate: Operate.TrackOperate): TrackControlApi {
        return TrackControlImpl(path = path, operate = operate, videoFormat = videoFormat!!).also {
            LogUtils.i(TAG, "getTrackManager")
        }
    }

    fun getCuttingManager(operate: Operate.EditingOperate): VideoEditingApi {
        return VideoEditingImpl(path = path, operate = operate, videoFormat = videoFormat!!).also {
            LogUtils.i(TAG, "getCuttingManager")
        }
    }

}