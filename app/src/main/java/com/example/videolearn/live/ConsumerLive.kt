package com.example.videolearn.live

import android.util.Log
import com.example.rtmplib.RtmpLib
import com.example.videolearn.MediaScope
import kotlinx.coroutines.launch
import java.util.concurrent.LinkedBlockingQueue

object ConsumerLive {
    private const val TAG = "ConsumerLive"
    private val queue = LinkedBlockingQueue<RTMPPackage>()
    private const val URL =
        "rtmp://live-push.bilivideo.com/live-bvc/?streamname=live_25244143_7799450&key=1d9dadad23e4413a6ffa85b3ff82dadb&schedule=rtmp&pflag=1"

    init {
        startLoopSendData()
    }

    fun connectService() {
        MediaScope.launch {
            val result = RtmpLib.connectBiliService(URL)
            Log.i(TAG, "connectService: $result")
        }
    }

    fun addPackage(rtmpPackage: RTMPPackage?) {
        queue.add(rtmpPackage)
    }

    private fun startLoopSendData() {
        MediaScope.launch {
            while (true) {
                val rtmpPackage = queue.take()
                if (rtmpPackage.buffer.isNotEmpty()) {
                    Log.i(TAG, "推流 type:${rtmpPackage.type} size:${rtmpPackage.buffer.size}")
                    RtmpLib.sendData(
                        rtmpPackage.buffer,
                        rtmpPackage.buffer.size,
                        rtmpPackage.times,
                        rtmpPackage.type
                    )
                }

            }
        }
    }
}