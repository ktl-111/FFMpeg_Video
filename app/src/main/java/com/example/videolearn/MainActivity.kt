package com.example.videolearn

import android.content.Intent
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.videolearn.ffmpegcompose.FFMpegComposeActivity
import com.example.videolearn.live.LiveActivity
import com.example.videolearn.shotscreen.ShotScreenActivity
import com.example.videolearn.test.ParseDataActivity
import com.example.videolearn.test.TestActivity
import com.example.videolearn.utils.ResultUtils
import com.example.videolearn.videocall.VideoCallActivity
import com.example.videolearn.videoplay.VideoPlayActiivty

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
        ResultUtils.getInstance(this)
            .permissions(
                arrayOf(
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    android.Manifest.permission.READ_EXTERNAL_STORAGE
                ), object : ResultUtils.PermissionsCallBack {
                    override fun deniedList(deniedList: MutableList<String>?) {
                    }

                    override fun grantedList(grantedList: MutableList<String>?) {

                    }
                }
            )
        startService(Intent(this, MediaService::class.java))
    }

    @Composable
    private fun rootView() {
        Column {
            button("test") {
                test()
            }
            button("投屏") {
                shotScreen()
            }
            button("视频通话") {
                video()
            }
            button("读取流显示") {
                parsedata()
            }
            button("videoplay") {
                videoplay()
            }
            button("直播") {
                live()
            }
            button("ffmpeg") {
                ffmpeg()
            }
        }
    }

    private fun ffmpeg() {
        startActivity(Intent(this, FFMpegComposeActivity::class.java))
    }

    fun test() {
        startActivity(Intent(this, TestActivity::class.java))
    }

    fun shotScreen() {
        startActivity(Intent(this, ShotScreenActivity::class.java))
    }

    fun video() {
        startActivity(Intent(this, VideoCallActivity::class.java))
    }

    fun parsedata() {
        startActivity(Intent(this, ParseDataActivity::class.java))
    }

    fun videoplay() {
        startActivity(Intent(this, VideoPlayActiivty::class.java))
    }

    fun live() {
//        startActivity(Intent(this, MainActivity::class.java))
        startActivity(Intent(this, LiveActivity::class.java))
    }

    @Composable
    private fun button(text: String, click: () -> Unit) = Button(
        onClick = click, modifier = Modifier
            .padding(10.dp)
            .fillMaxWidth()
    ) {
        Text(text = text)
    }

}
