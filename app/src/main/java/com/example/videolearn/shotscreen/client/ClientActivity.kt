package com.example.videolearn.shotscreen.client

import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R

class ClientActivity : AppCompatActivity(), SurfaceHolder.Callback {
    lateinit var etIp: EditText
    lateinit var etProt: EditText
    lateinit var surfaceView: SurfaceView
    var projectionH264Connect: ProjectionH264Connect? = null
    var surfaceHolder: SurfaceHolder? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.actiivty_ss_client)
        etIp = findViewById(R.id.et_ip)
        etProt = findViewById(R.id.et_prot)
        surfaceView = findViewById(R.id.sv_client)
        surfaceView.holder.addCallback(this)
    }

    fun connect(v: View) {
        surfaceHolder?.also {
            projectionH264Connect = ProjectionH264Connect(
                it.surface,
                etIp.text.toString().trim(),
                etProt.text.toString().trim()
            )
                .apply {
                    start()
                }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        projectionH264Connect?.release()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceHolder = holder
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }
}