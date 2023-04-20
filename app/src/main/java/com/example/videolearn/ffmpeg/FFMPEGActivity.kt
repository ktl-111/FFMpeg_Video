package com.example.videolearn.ffmpeg

import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup.LayoutParams
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.nativelib.FFMpegPlay
import java.io.File

class FFMPEGActivity : AppCompatActivity() {
    private lateinit var surface: Surface
    private lateinit var surfaceView: SurfaceView
    private var ffMpegPlay: FFMpegPlay? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { rootView() }

    }

    private fun play() {
        //直播地址
//        val path = "http://zhibo.hkstv.tv/livestream/mutfysrq/playlist.m3u8"
//        val path = "http://39.135.138.58:18890/PLTV/88888888/224/3221225630/index.m3u8"
        val path = File(application.externalCacheDir, "ffmpeg.mp4").absolutePath
        ffMpegPlay ?: let {
            FFMpegPlay(surfaceView).apply { ffMpegPlay = this }
        }.play(path, surface = surface)

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
            Button(
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
                    .align(alignment = Alignment.BottomCenter),
                onClick = { play() }) {
                Text(text = "play")
            }
        }
    }

}