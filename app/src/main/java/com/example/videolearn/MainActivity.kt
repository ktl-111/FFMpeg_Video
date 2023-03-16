package com.example.videolearn

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.videoplay.VideoPlayActiivty
import com.example.videolearn.shotscreen.ShotScreenActivity
import com.example.videolearn.test.ParseDataActivity
import com.example.videolearn.test.TestActivity
import com.example.videolearn.videocall.VideoCallActivity

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        startService(Intent(this, MediaService::class.java))
    }

    fun test(v: View) {
        startActivity(Intent(this, TestActivity::class.java))
    }

    fun ShotScreen(v: View) {
        startActivity(Intent(this, ShotScreenActivity::class.java))
    }

    fun video(v: View) {
        startActivity(Intent(this, VideoCallActivity::class.java))
    }

    fun parsedata(v: View) {
        startActivity(Intent(this, ParseDataActivity::class.java))
    }

    fun videoplay(v: View) {
        startActivity(Intent(this, VideoPlayActiivty::class.java))
    }
}
