package com.example.play

import android.util.Log
import android.view.Surface
import com.example.play.PlayerState
import com.example.play.config.OutConfig
import com.example.play.proxy.FFMpegProxy

class PlayManager : IPaly {
    private val TAG = "PlayManager"
    private lateinit var mProxy: IPaly
    private var iPalyListener: IPalyListener? = null
    override fun init(iPalyListener: IPalyListener?) {
        this.iPalyListener = iPalyListener
    }

    override fun prepare(path: String, surface: Surface, outConfig: OutConfig?) {
        if (path.isEmpty()) {
            Log.i(TAG, "prepare path is empty")
            return
        }
        mProxy = FFMpegProxy()
        mProxy.init(iPalyListener)
        mProxy.prepare(path, surface, outConfig)
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

    override fun seekTo(seekTime: Long) {
        if (this::mProxy.isInitialized) {
            mProxy.seekTo(seekTime)
        }
    }

    override fun surfaceReCreate(surface: Surface) {
        if (this::mProxy.isInitialized) {
            mProxy.surfaceReCreate(surface)
        }
    }

    override fun surfaceDestroy() {
        if (this::mProxy.isInitialized) {
            mProxy.surfaceDestroy()
        }
    }

    override fun getPlayerState(): PlayerState {
        return if (this::mProxy.isInitialized) {
            mProxy.getPlayerState()
        } else {
            PlayerState.Unknown
        }
    }

    override fun getCurrTimestamp(): Long {
        return if (this::mProxy.isInitialized) {
            mProxy.getCurrTimestamp()
        } else {
            0
        }
    }
}