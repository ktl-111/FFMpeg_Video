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
    private lateinit var liveProjectionH264Encoder: LiveProjectionH264Encoder
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
        liveProjectionH264Encoder = LiveProjectionH264Encoder(this)
    }

    private fun connect() {
        liveProjectionH264Encoder.connectService()
    }

    private fun startProjection() {
        ResultUtils.getInstance(this)
            .singlePermissions(
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE
            ) { granted ->
                if (granted) {
                    liveProjectionH264Encoder.startProjection()
                }
            }
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