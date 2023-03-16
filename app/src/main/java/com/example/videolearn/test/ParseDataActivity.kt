package com.example.videolearn.test

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Bundle
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R
import com.example.videolearn.VideoScope
import com.example.videolearn.utils.H264Codec
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File

class ParseDataActivity : AppCompatActivity(), SurfaceHolder.Callback {
    private val TAG = "ParseDataActivity"
    val filePath by lazy { File(application.externalCacheDir, "parsedata.h264") }
    lateinit var surfaceView: SurfaceView
    var surfaceHolder: SurfaceHolder? = null
    var mediaCodec: MediaCodec? = null


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_parsedata)
        surfaceView = findViewById(R.id.parse_sv)
        surfaceView.holder.addCallback(this)
    }


    fun fore(v: View) {
        surfaceHolder?.also { holder ->
            VideoScope.launch {
                val startCodec = H264Codec(filePath).startCodec()
                startCodec?.also {
                    var spsData: ByteArray? = null
                    var ppsData: ByteArray? = null
                    Log.i(TAG, "fore size:${it.size}")
                    for (resultData in it) {
                        if (resultData.isSps()) {
                            Log.i(
                                TAG,
                                "fore: SPS width:${resultData.width} height:${resultData.height}"
                            )
                            if (spsData == null) {
                                spsData = resultData.resultData
                                initMediaCodec(resultData.width, resultData.height, holder)
                            }
                        } else if (resultData.isPPS()) {
                            spsData?.also { spsData ->
                                Log.i(TAG, "fore: PPS")
                                if (ppsData == null) {
                                    ppsData = resultData.resultData
                                    val byteArray = ByteArray(spsData.size + ppsData!!.size)
                                    System.arraycopy(spsData, 0, byteArray, 0, spsData.size)
                                    System.arraycopy(
                                        ppsData,
                                        0,
                                        byteArray,
                                        spsData.size,
                                        ppsData!!.size
                                    )
                                    parseData(byteArray)
                                }
                            }
                        } else if (resultData.isI()) {
                            Log.i(TAG, "fore: I帧")
                            parseData(resultData.resultData)
                        } else {
                            Log.i(TAG, "fore: 其他帧")
                            parseData(resultData.resultData)
                        }
                        delay(50)
                    }
                }
            }
        }

    }

    fun initMediaCodec(width: Int, height: Int, surface: SurfaceHolder) {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
        format.setInteger(MediaFormat.KEY_BIT_RATE, width * height)
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 30)
        //来源
        format.setInteger(
            MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
        )
        mediaCodec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC).apply {
            configure(
                format,
                surface.surface,
                null, 0
            )
            start()
        }
    }

    fun parseData(data: ByteArray) {
        mediaCodec?.also { mediaCodec ->
            Log.i(TAG, "parseData: ")
            val dequeueInputBuffer = mediaCodec.dequeueInputBuffer(10_000)
            Log.i(TAG, "parseData  dequeueInputBuffer:$dequeueInputBuffer")
            if (dequeueInputBuffer >= 0) {
                val inputBuffer = mediaCodec.getInputBuffer(dequeueInputBuffer)
                inputBuffer?.also {
                    it.clear()
                    it.put(data, 0, data.size)
                }
                //pts为当前时间戳,测试
                mediaCodec.queueInputBuffer(
                    dequeueInputBuffer,
                    0,
                    data.size,
                    0,
                    0
                )
            }
            val bufferInfo = MediaCodec.BufferInfo()
            var dequeueOutputBuffer = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
            Log.i(TAG, "parseData  dequeueOutputBuffer:$dequeueOutputBuffer")
            //阻塞回调,一直等待解码完成
            while (dequeueOutputBuffer >= 0) {
                mediaCodec.releaseOutputBuffer(dequeueOutputBuffer, true)
                dequeueOutputBuffer = mediaCodec.dequeueOutputBuffer(bufferInfo, 1_000)
                Log.i(TAG, "parseData while dequeueOutputBuffer:$dequeueOutputBuffer")
            }
        } ?: kotlin.run {
            Log.e(TAG, "parseData: mediaCodec is null")
        }

    }

    fun first(v: View) {
        surfaceHolder?.also { holder ->
            VideoScope.launch {
                val startCodec = H264Codec(filePath).startCodec()
                startCodec?.also {
                    var spsData: ByteArray? = null
                    var ppsData: ByteArray? = null
                    val maxI = 2
                    var currI = 0
                    //只传spspps和I,无法显示画面,由于有缓冲区,需要多刷几帧才行
                    val maxO = 3
                    var currO = 0
                    Log.i(TAG, "fore size:${it.size}")
                    for (resultData in it) {
                        if (resultData.isSps()) {
                            Log.i(
                                TAG,
                                "fore: SPS width:${resultData.width} height:${resultData.height}"
                            )
                            if (spsData == null) {
                                spsData = resultData.resultData
                                initMediaCodec(resultData.width, resultData.height, holder)
                            }
                        } else if (resultData.isPPS()) {
                            spsData?.also { spsData ->
                                Log.i(TAG, "fore: PPS")
                                if (ppsData == null) {
                                    ppsData = resultData.resultData
                                    val byteArray = ByteArray(spsData.size + ppsData!!.size)
                                    System.arraycopy(spsData, 0, byteArray, 0, spsData.size)
                                    System.arraycopy(
                                        ppsData,
                                        0,
                                        byteArray,
                                        spsData.size,
                                        ppsData!!.size
                                    )
                                    parseData(byteArray)
                                }
                            }
                        } else if (resultData.isI()) {
                            Log.i(TAG, "fore: I帧")
                            parseData(resultData.resultData)
                            if (currI >= maxI) {
                                return@also
                            }
                            currI += 1
                        } else {
                            Log.i(
                                TAG,
                                "fore: 其他帧 currO:${currO} slice_type:${resultData.slice_type}"
                            )
                            parseData(resultData.resultData)
                            if (currO >= maxO) {
                                return@also
                            }
                            currO += 1
                        }
                        delay(500)
                    }
                }

            }
        }

    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceHolder = holder
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }

}