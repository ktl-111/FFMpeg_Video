package com.example.videolearn.live

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.videolearn.utils.ResultUtils

class LiveActivity : AppCompatActivity() {
    private lateinit var liveProjectionEncoder: LiveProjectionEncoder
    private lateinit var liveAudioEncoder: LiveAudioEncoder
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
        liveProjectionEncoder = LiveProjectionEncoder(this)
        liveAudioEncoder = LiveAudioEncoder(this)
    }

    private fun connect() {
        ConsumerLive.connectService()
    }

    private fun startProjection() {
        ResultUtils.getInstance(this)
            .permissions(
                arrayOf(
                    android.Manifest.permission.RECORD_AUDIO,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                ), object : ResultUtils.PermissionsCallBack {
                    override fun deniedList(deniedList: MutableList<String>?) {
                    }

                    override fun grantedList(grantedList: MutableList<String>?) {
                        liveProjectionEncoder.startProjection()
                        liveAudioEncoder.startAudioRecord()
                    }
                })
    }

    @Composable
    fun rootView() {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
        ) {
            Button(
                onClick = { connect() },
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
            ) {
                Text(text = "Connect")
            }
            Button(
                onClick = { startProjection() },
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
            ) {
                Text(text = "Start Live")
            }
        }
    }


    @Preview(showBackground = true)
    @Composable
    fun test() {
        rootView()
    }

}