package com.example.videolearn.glvideo

import android.graphics.Bitmap
import android.media.MediaFormat
import android.os.Bundle
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.Button
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.play.config.OutConfig
import com.example.videolearn.BaseActivity
import com.example.videolearn.play.EditingCallback
import com.example.videolearn.play.Operate
import com.example.videolearn.play.PlayCallback
import com.example.videolearn.play.PlaybackControlApi
import com.example.videolearn.play.VideoControlApi
import com.example.videolearn.play.VideoManager
import com.example.videolearn.play.VideoTrackCallback
import com.norman.android.hdrsample.util.LogUtils
import com.norman.android.hdrsample.util.MediaFormatUtil
import com.norman.android.hdrsample.util.toBitmap
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer

class GLVideoActivity : BaseActivity() {
    private val TAG = "GLVideoActivity"
    private val managerList = mutableMapOf<String, VideoControlApi>()

    private val videoList = mutableStateListOf<BitmapBean>()
    private lateinit var videoManager: VideoManager
    private val trackItemSize = 40

    data class BitmapBean(val bitmap: Bitmap, val time: Long)

    private lateinit var srcPath: String
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        srcPath = intent.getStringExtra("filepath") ?: ""
        videoManager = VideoManager(srcPath)
        setContent {
            Column {
                Play(srcPath)
                Track(srcPath)
                Button(modifier = Modifier.fillMaxWidth(), onClick = {
                    cutting()
                }) {
                    Text(text = "cutting")
                }
            }
        }
    }

    private fun cutting() {
        managerList.values.forEach {
            if (it is PlaybackControlApi) {
                it.pause()
            }
        }
        val outFile = File(getExternalFilesDir(""), "testout.mp4")
        if (!outFile.exists()) {
            try {
                outFile.createNewFile()
            } catch (e: IOException) {
                throw RuntimeException(e)
            }
        } else {
            outFile.delete()
        }
        val destPath = outFile.absolutePath
        val startTime: Long = 0
        val allTime: Long = 5000
        val cuttingManager = videoManager.getCuttingManager(Operate.EditingOperate(OutConfig(fps = 24, scale = 0.3), destPath, startTime, allTime, object : EditingCallback {
            override fun onStart() {
                LogUtils.i(TAG, "cutting start")
            }

            override fun onEditingProgress(progress: Double) {
                LogUtils.i(TAG, "cutting onCuttingProgress:${progress}")
            }

            override fun onEditingDone() {
                LogUtils.i(TAG, "cutting onCuttingDone")
            }

            override fun onFail(errorCode: Int) {
                LogUtils.i(TAG, "cutting onFail:${errorCode}")
            }

        }))
        cuttingManager.start()
    }

    @Composable
    private fun Track(path: String) {
        val videoFormat = videoManager.videoFormat
        val duration = MediaFormatUtil.getLong(videoFormat, MediaFormat.KEY_DURATION) / 1000.0 / 1000.0
        val trackManager = videoManager.getTrackManager(Operate.TrackOperate(OutConfig(scale = 0.1), object : VideoTrackCallback {
            override fun onVideoTrackUpdate(byteBuffer: ByteBuffer, width: Int, height: Int, time: Long) {
                videoList.add(BitmapBean(byteBuffer.toBitmap(width, height), (time)))
            }

            override fun videoTrackInterval(videoDuration: Long): LongArray {
                val size = (Math.ceil(duration).toInt() + 1)
                Log.i(TAG, "onStart duration:${duration} size:$size")
                val ptsArrays = LongArray(size)
                for (i in 0 until size) {
                    ptsArrays[i] = (i.toDouble() * 1000).toLong()
                }
                return ptsArrays
            }

        }))
        managerList["track"] = trackManager
        trackManager.start()
        TrackContent()
    }

    @Composable
    private fun TrackContent() {
        LazyRow(modifier = Modifier
            .fillMaxWidth()
            .wrapContentHeight()) {
            itemsIndexed(videoList, key = { index, item -> "${index}-${item.hashCode()}" }) { _, bitmap ->
                Column {
                    Text(text = "${bitmap.time}", color = Color.Red, fontSize = 10.sp)
                    Card(modifier = Modifier
                        .width(trackItemSize.dp)
                        .aspectRatio(1f)) {
                        Image(painter = BitmapPainter(bitmap.bitmap.asImageBitmap()), contentDescription = "",
                            modifier = Modifier
                                .fillMaxSize(),
                            contentScale = ContentScale.Crop)
                    }
                }
            }
        }
    }


    @Composable
    private fun Play(path: String) {
        Box(modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)) {
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { context ->
                    return@AndroidView SurfaceView(context).also {
                        it.holder.addCallback(object : SurfaceHolder.Callback {
                            override fun surfaceCreated(holder: SurfaceHolder) {
                                Log.i(TAG, "surfaceCreated: ")
                                val playManager = videoManager.getPlayManager(Operate.PlayOperate(surface = holder.surface, playCallback = object : PlayCallback {
                                    override fun onPlayProgress(time: Long) {
                                        LogUtils.i(TAG, "play onPlayProgress${time}")
                                    }

                                    override fun onPlayComplete() {
                                        LogUtils.i(TAG, "play onPlayComplete")
                                    }

                                }))
                                managerList["play"] = playManager
                                playManager.start()
                            }

                            override fun surfaceChanged(
                                holder: SurfaceHolder, format: Int, width: Int, height: Int
                            ) {
                            }

                            override fun surfaceDestroyed(holder: SurfaceHolder) {
                                Log.i(TAG, "surfaceDestroyed: ")
                            }

                        })
                        it.layoutParams =
                            ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT
                            )
                    }
                }
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        managerList.values.forEach {
            it.stop()
        }
    }

    override fun onResume() {
        super.onResume()
        managerList.values.forEach {
            if (it is PlaybackControlApi) {
                it.resume()
            }
        }
    }

    override fun onPause() {
        super.onPause()
        managerList.values.forEach {
            if (it is PlaybackControlApi) {
                it.pause()
            }
        }
    }
}