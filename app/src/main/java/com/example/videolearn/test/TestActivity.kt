package com.example.videolearn.test

import android.os.Bundle
import android.view.SurfaceView
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R
import com.example.videolearn.utils.ResultUtils


class TestActivity : AppCompatActivity() {
    lateinit var surfaceView: SurfaceView
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        surfaceView = findViewById(R.id.sv_camera)
    }

    fun startProjection(view: View) {
        ResultUtils.getInstance(this)
            .singlePermissions(
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE
            ) { granted ->
                if (granted) {
                    ProjectionH264Encoder(this).startProjection()
                }
            }
    }

    fun startCamera(view: View) {
        ResultUtils.getInstance(this)
            .singlePermissions(
                android.Manifest.permission.CAMERA
            ) { granted ->
                if (granted) {
                    CameraH264Encoder(this, surfaceView)
                        .startCamera()
                }
            }
    }
}
