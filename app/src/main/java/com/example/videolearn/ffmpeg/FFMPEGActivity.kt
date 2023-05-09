package com.example.videolearn.ffmpeg

import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup.LayoutParams
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.*
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.nativelib.FFMpegPlay
import com.example.videolearn.MediaScope
import com.example.videolearn.utils.FileUtils
import kotlinx.coroutines.launch
import java.io.File

class FFMPEGActivity : AppCompatActivity() {
    val TAG = "FFMPEGActivity"
    private lateinit var surface: Surface
    private lateinit var surfaceView: SurfaceView
    private var ffMpegPlay: FFMpegPlay? = null
    private var path: String? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { rootView() }

    }

    private fun play() {

        //直播地址
//         path = "http://zhibo.hkstv.tv/livestream/mutfysrq/playlist.m3u8"
//         path = "http://39.135.138.58:18890/PLTV/88888888/224/3221225630/index.m3u8"
//        path = File(application.externalCacheDir, "testout_no_first_i.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "douyin.mp4").absolutePath
        path = File(Environment.getExternalStorageDirectory(), "video.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "ffmpeg.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "VID_20230511_004231.mp4").absolutePath
//        path = File(application.externalCacheDir, "vid_test1.mp4").absolutePath
        val path = path!!
        if (!path.startsWith("http")) {
            if (!File(path).exists()) {
                Toast.makeText(this, "文件不存在", Toast.LENGTH_SHORT).show()
                return
            }
        }

        ffMpegPlay ?: let {
            FFMpegPlay(surfaceView).apply {
                ffMpegPlay = this
                callback = {
                    FileUtils.writeBytes(it, "ffmpeg")
                    FileUtils.writeContent(it, "ffmpeg")
                }
            }
        }.play(path, surface = surface)
    }

    private fun pause() {
        ffMpegPlay?.release()
    }

    private fun cutting() {
        path?.also {
            if (it.isNotEmpty() && !it.startsWith("http")) {
                MediaScope.launch {
                    val outFile = File(application.externalCacheDir, "testout.mp4")
                    if (!outFile.exists()) {
                        outFile.createNewFile()
                    } else {
                        outFile.delete()
                    }
                    Log.i(TAG, "cutting file:${outFile.absolutePath}")
                    val destPath = outFile.absolutePath
                    ffMpegPlay?.cutting(destPath)
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        ffMpegPlay?.release()
    }

    @Preview
    @Composable
    fun rootView() {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
        ) {
            AndroidView(factory = { context ->
                return@AndroidView SurfaceView(context).also {
                    surfaceView = it
                    it.holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            surface = holder.surface
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                        }

                    })
                    it.layoutParams =
                        LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT)
                }
            })
            Column(
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
                    .align(alignment = Alignment.BottomCenter)
            ) {
                Button(
                    modifier = Modifier
                        .padding(10.dp)
                        .fillMaxWidth(),
                    onClick = { cutting() }) {
                    Text(text = "cutting")
                }
                Button(
                    modifier = Modifier
                        .padding(10.dp)
                        .fillMaxWidth(),
                    onClick = { pause() }) {
                    Text(text = "pause")
                }
                Button(
                    modifier = Modifier
                        .padding(10.dp)
                        .fillMaxWidth(),
                    onClick = { play() }) {
                    Text(text = "play")
                }
            }

        }
    }


}