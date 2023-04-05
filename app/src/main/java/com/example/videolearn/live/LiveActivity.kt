package com.example.videolearn.live

import android.os.Bundle
import android.view.ViewGroup
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.view.PreviewView
import androidx.compose.foundation.layout.*
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.videolearn.utils.ResultUtils

class LiveActivity : AppCompatActivity() {
    private lateinit var previewView: PreviewView
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
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
                        LiveProjectionEncoder(this@LiveActivity).startProjection()
                        LiveAudioEncoder(this@LiveActivity).startAudioRecord()
                    }
                })
    }

    private fun startCamera() {
        ResultUtils.getInstance(this)
            .permissions(
                arrayOf(
                    android.Manifest.permission.RECORD_AUDIO,
                    android.Manifest.permission.CAMERA,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                ), object : ResultUtils.PermissionsCallBack {
                    override fun deniedList(deniedList: MutableList<String>?) {
                    }

                    override fun grantedList(grantedList: MutableList<String>?) {
                        LiveCameraEncoder(this@LiveActivity, previewView)
                            .startCamera()
                        LiveAudioEncoder(this@LiveActivity).startAudioRecord()
                    }
                })
    }

    @Composable
    fun rootView() {
        Box {
            AndroidView(
                factory = {
                    return@AndroidView PreviewView(it).apply {
                        previewView = this
                        layoutParams = ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT
                        )
                    }
                }, modifier = Modifier
                    .fillMaxWidth()
                    .fillMaxHeight()
            )
            Column(
                modifier = Modifier.align(Alignment.TopEnd),
                horizontalAlignment = Alignment.End
            ) {
                buttonView("Connect") {
                    connect()
                }

                buttonView("Start Live By Projection") {
                    startProjection()
                }
                buttonView("Start Live By Camera") {
                    startCamera()
                }
            }
        }
    }

    @Composable
    fun buttonView(text: String, onClick: () -> Unit) {
        Button(
            onClick = onClick,
            modifier = Modifier
                .padding(10.dp)
        ) {
            Text(text = text)
        }
    }


    @Preview(showBackground = true)
    @Composable
    fun test() {
        rootView()
    }

}