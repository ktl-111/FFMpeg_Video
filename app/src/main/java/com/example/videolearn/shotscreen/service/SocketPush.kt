package com.example.videolearn.shotscreen.service

import android.util.Log
import android.widget.Toast
import com.example.videolearn.App
import org.java_websocket.WebSocket
import org.java_websocket.handshake.ClientHandshake
import org.java_websocket.server.WebSocketServer
import java.lang.Exception
import java.net.InetSocketAddress

private const val TAG = "SocketPush"

class SocketPush(private val port: String) {
    private var webSocket: WebSocket? = null
    private val webSocketServer: WebSocketServer =
        object : WebSocketServer(InetSocketAddress(port.toInt())) {
            override fun onOpen(webSocket: WebSocket, handshake: ClientHandshake) {
                App.runOnUi {
                    Toast.makeText(App.application, "push open", Toast.LENGTH_SHORT).show()
                }
                this@SocketPush.webSocket = webSocket
            }

            override fun onClose(conn: WebSocket?, code: Int, reason: String?, remote: Boolean) {
                Log.i(TAG, "onClose: ")
            }

            override fun onMessage(conn: WebSocket?, message: String?) {
                Log.i(TAG, "onMessage: ")
            }

            override fun onError(conn: WebSocket, ex: Exception) {
                Log.e(TAG, "onError: ", ex)
            }

            override fun onStart() {
                Log.i(TAG, "onStart: ")
            }

        }

    fun start() {
        webSocketServer.start()
    }

    fun sendData(bytes: ByteArray) {
        Log.i(TAG, "sendData: ${bytes.size}")
        webSocket?.also {
            if (it.isOpen) {
                Log.i(TAG, "sendData success")
                it.send(bytes)
            }
        }
    }

    fun close() {
        kotlin.runCatching {
            webSocket?.close()
            webSocketServer.stop()
        }.onFailure {
            it.printStackTrace()
        }
    }
}