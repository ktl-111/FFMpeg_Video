package com.example.videolearn.shotscreen.client

import android.util.Log
import android.widget.Toast
import com.example.videolearn.App
import org.java_websocket.client.WebSocketClient
import org.java_websocket.handshake.ServerHandshake
import java.net.URI
import java.nio.ByteBuffer

private const val TAG = "SocketConnect"

class SocketConnect(
    private val ip: String,
    private val prot: String,
    private val socketCallback: SocketCallback
) {
    private var myWebSocketClient: MyWebSocketClient? = null

    fun start() {
        val url = URI("ws://$ip:$prot")
        myWebSocketClient = MyWebSocketClient(url).apply {
            connect()
        }
    }

    fun stop() {
        myWebSocketClient?.close()
    }

    private inner class MyWebSocketClient(serverURI: URI?) : WebSocketClient(serverURI) {
        override fun onOpen(serverHandshake: ServerHandshake) {
            Log.i(TAG, "打开 socket  onOpen: ")
            App.runOnUi {
                Toast.makeText(App.application, "start receive", Toast.LENGTH_SHORT).show()
            }
        }

        override fun onMessage(s: String) {}

        override fun onMessage(bytes: ByteBuffer) {
            Log.i(TAG, "消息lenght:" + bytes.remaining())
            val buf = ByteArray(bytes.remaining())
            bytes.get(buf)
            socketCallback.callBack(buf)
        }

        override fun onClose(i: Int, s: String, b: Boolean) {
            Log.i(TAG, "onClose: ")
        }

        override fun onError(e: Exception) {
            Log.i(TAG, "onError: ${e.message}")
            e.printStackTrace()
        }
    }

    interface SocketCallback {
        fun callBack(data: ByteArray)
    }
}