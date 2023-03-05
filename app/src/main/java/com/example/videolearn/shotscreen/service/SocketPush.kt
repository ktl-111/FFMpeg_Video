package com.example.videolearn.shotscreen.service

import android.util.Log
import org.java_websocket.WebSocket
import org.java_websocket.handshake.ClientHandshake
import org.java_websocket.server.WebSocketServer
import java.lang.Exception
import java.net.InetSocketAddress
private const val TAG = "SocketPush"
class SocketPush(val prot: String) {
    var webSocket: WebSocket? = null
    private val webSocketServer: WebSocketServer =
        object : WebSocketServer(InetSocketAddress(prot.toInt())) {
            override fun onOpen(webSocket: WebSocket, handshake: ClientHandshake) {
                this@SocketPush.webSocket = webSocket
            }

            override fun onClose(conn: WebSocket?, code: Int, reason: String?, remote: Boolean) {
            }

            override fun onMessage(conn: WebSocket?, message: String?) {
            }

            override fun onError(conn: WebSocket?, ex: Exception?) {
            }

            override fun onStart() {
            }

        }

   fun start(){
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