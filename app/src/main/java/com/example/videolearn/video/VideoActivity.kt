package com.example.videolearn.video

import android.os.Bundle
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R
import com.example.videolearn.utils.ResultUtils

class VideoActivity : AppCompatActivity() {
    val TAG = "VideoActivity"
    lateinit var etPeerIpPort: EditText
    lateinit var etSelfPort: EditText
    lateinit var peerSufaceView: SurfaceView
    lateinit var selfSufaceView: SurfaceView
    var h264Push: H264Push? = null
    var h264Receive: H264Receive? = null
    var surfaceHolder: SurfaceHolder? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video)
        etPeerIpPort = findViewById(R.id.et_peer_ip_port)
        etSelfPort = findViewById(R.id.et_self_port)
        peerSufaceView = findViewById(R.id.sv_peer)
        selfSufaceView = findViewById(R.id.sv_self)
        findViewById<View>(R.id.bt_connet).setOnClickListener {
            connect()
        }
        findViewById<View>(R.id.bt_broadcast).setOnClickListener {
            broadcast()
        }

        peerSufaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                surfaceHolder = holder

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
    }

    private fun broadcast() {
        ResultUtils.getInstance(this)
            .singlePermissions(android.Manifest.permission.CAMERA) {
                if (it) {
                    h264Push = H264Push(this, selfSufaceView).apply {
                        startCamera()
                        startBroadcast(etSelfPort.text.toString().trim())
                    }
                } else {
                    Log.e(TAG, "onCreate: not permission")
                }
            }
    }

    private fun connect() {
        val split = etPeerIpPort.text.toString().trim().split(" ")
        val ip = split[0]
        val port = split[1]
        surfaceHolder?.also { sh ->
            h264Receive ?: let {
                h264Receive = H264Receive(sh.surface, ip, port)
                h264Receive
            }
            h264Receive?.startConnect()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        h264Push?.release()
        h264Receive?.release()
    }
}