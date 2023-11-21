package com.example.play

import android.util.Log
import android.view.Surface
import com.example.play.proxy.FFMpegProxy

class PlayManager : IPaly {
    private val TAG = "PlayManager"
    private lateinit var mProxy: IPaly
    private var iPalyListener:IPalyListener?=null
    override fun init(iPalyListener: IPalyListener?) {
        this.iPalyListener = iPalyListener
    }

    override fun perpare(path: String, surface: Surface) {
        if (path.isEmpty()) {
            Log.i(TAG, "perpare path is empty")
            return
        }
        mProxy = FFMpegProxy()
        mProxy.init(iPalyListener)
        mProxy.perpare(path, surface)
    }

    override fun start() {
        if (this::mProxy.isInitialized) {
            mProxy.start()
        }
    }

    override fun stop() {
        if (this::mProxy.isInitialized) {
            mProxy.stop()
        }
    }

    override fun resume() {
        if (this::mProxy.isInitialized) {
            mProxy.resume()
        }
    }

    override fun pause() {
        if (this::mProxy.isInitialized) {
            mProxy.pause()
        }
    }

    override fun release() {
        if (this::mProxy.isInitialized) {
            mProxy.release()
        }
    }

    override fun seekTo(seek: Double) {
        if (this::mProxy.isInitialized) {
            mProxy.seekTo(seek)
        }
    }

}