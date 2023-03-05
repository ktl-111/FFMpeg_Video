package com.example.videolearn.shotscreen

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R
import com.example.videolearn.shotscreen.client.ClientActivity
import com.example.videolearn.shotscreen.service.ServiceActivity

class ShotScreenActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_shotscreen)
    }

    fun client(v: View) {
        startActivity(Intent(this, ClientActivity::class.java))
    }

    fun service(v: View) {
        startActivity(Intent(this, ServiceActivity::class.java))
    }
}