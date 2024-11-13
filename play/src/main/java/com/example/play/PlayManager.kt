package com.example.play

import android.view.Surface
import com.example.play.config.OutConfig
import com.example.play.proxy.FFMpegProxy
import com.example.play.utils.FFMpegUtils
import com.example.play.utils.LogHelper
import com.example.play.utils.MediaScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch

class PlayManager : IPaly {
    private val TAG = "PlayManager"
    private lateinit var mProxy: IPaly
    private var iPalyListener: IPalyListener? = null
    private val seekDoneFlow by lazy {
        MutableStateFlow(true)
    }
    private var callStop = false
    private val seekFlow by lazy {
        MutableSharedFlow<SeekBean>(1, 0, BufferOverflow.DROP_OLDEST).also { flow ->
            MediaScope.launch(Dispatchers.Default) {
                flow.distinctUntilChanged { old, new ->
                    if (old.seekTime == 0L && new.seekTime == 0L) {
                        false
                    } else {
                        old == new
                    }
                }.onEach { seekBean ->
                    LogHelper.i(TAG, "flow seek start: ${seekBean},callStop:$callStop")
                    if (callStop) {
                        return@onEach;
                    }
                    seekDoneFlow.value = false
                    mProxy.seekTo(seekBean.seekTime)
                    seekDoneFlow.value = true
                    LogHelper.i(TAG, "flow seek end: ${seekBean}")
                    nextStep(seekBean.nextStep)
                }.collect()
            }
        }
    }

    data class SeekBean(val seekTime: Long, val nextStep: Step)

    override fun init(iPalyListener: IPalyListener?) {
        this.iPalyListener = iPalyListener
    }

    override fun prepare(path: String, surface: Surface, outConfig: OutConfig?) {
        if (path.isEmpty()) {
            LogHelper.i(TAG, "prepare path is empty")
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
            MediaScope.launch(Dispatchers.IO) {
                callStop = true
                val seekResult = seekDoneFlow.value
                LogHelper.i(TAG, "stop,curr seekDoneFlow:$seekResult")
                if (!seekResult) {
                    seekDoneFlow.onEach { result ->
                        LogHelper.i(TAG, "stop seekDoneFlow result:$result")
                        if (result) {
                            mProxy.stop()
                        }
                    }.collect()
                } else {
                    mProxy.stop()
                }
            }
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

    override fun seekTo(seekTime: Long, nextStep: Step) {
        if (this::mProxy.isInitialized) {
            MediaScope.launch(Dispatchers.IO) {
                seekFlow.emit(SeekBean(seekTime, nextStep))
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

    private fun nextStep(step: Step) {
        LogHelper.i(TAG, "nextStep: ${step}")
        when (step) {
            Step.PauseStep -> pause()
            Step.PlayStep -> start()
            Step.UnknownStep -> {}
        }
    }

    override fun cutting(srcPath: String, destPath: String, startTime: Long, endTime: Long, outConfig: OutConfig?, cb: FFMpegUtils.VideoCuttingInterface) {
        if (this::mProxy.isInitialized) {
            mProxy.cutting(srcPath, destPath, startTime, endTime, outConfig, cb)
        }
    }
}