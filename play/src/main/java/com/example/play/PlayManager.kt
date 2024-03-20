package com.example.play

import android.util.Log
import android.view.Surface
import com.example.play.PlayerState
import com.example.play.config.OutConfig
import com.example.play.proxy.FFMpegProxy
import com.example.play.utils.MediaScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch

class PlayManager : IPaly {
    private val TAG = "PlayManager"
    private lateinit var mProxy: IPaly
    private var iPalyListener: IPalyListener? = null
    private val seekFlow by lazy {
        MutableSharedFlow<Long>(1, 0, BufferOverflow.DROP_OLDEST)
            .also { flow ->
                MediaScope.launch(Dispatchers.Default) {
                    flow.distinctUntilChanged().onEach { seekTime ->
                        Log.i(TAG, "flow seek: ${seekTime}")
                        mProxy.seekTo(seekTime)
                    }.collect()
                }
            }
    }

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
            MediaScope.launch(Dispatchers.IO) {
                seekFlow.emit(seekTime)
            }
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