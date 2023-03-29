package com.example.videolearn.live

import android.annotation.SuppressLint
import android.content.Context
import android.media.*
import android.media.MediaCodec.BufferInfo
import com.example.videolearn.MediaScope
import kotlinx.coroutines.launch
import java.nio.ByteBuffer

@SuppressLint("MissingPermission")
class LiveAudioEncoder(context: Context) {
    private val TAG = "AudioEncoder"
    private val sampleRate = 44100
    private val channelCount = 1
    private val bitRate = 32_000
    private lateinit var audioRecord: AudioRecord
    private lateinit var audioCodec: MediaCodec
    private var minBufferSize = 0
    private fun initMediaConfig() {


        //采样率,声道数,量化格式
        minBufferSize = AudioRecord.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        //音频采集,
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            sampleRate,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT,
            minBufferSize
        )

        val audioFormat =
            MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, sampleRate, channelCount)
        //录音等级
        audioFormat.setInteger(
            MediaFormat.KEY_AAC_PROFILE,
            MediaCodecInfo.CodecProfileLevel.AACObjectLC
        )
        //比特率
        audioFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitRate)
        //设置最大size
        audioFormat.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, minBufferSize * 2)

        //创建音频编码
        audioCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC)
        audioCodec.configure(audioFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)

        audioCodec.start()
    }

    fun startAudioRecord() {
        initMediaConfig()
        startLoopAudioRecord()
    }

    private fun startLoopAudioRecord() {
        MediaScope.launch {
            //在获取播放的音频数据之前,发送audio Special config
            ConsumerLive.addPackage(RTMPPackage(byteArrayOf(0x12, 0x08), 0, RTMP_PACKET_TYPE_HEAD))
            kotlin.runCatching {
                var startTime: Long = 0
                audioRecord.startRecording()
                val audioByte = ByteArray(minBufferSize)
                while (true) {
                    //读取到音频数据
                    val len = audioRecord.read(audioByte, 0, minBufferSize)
                    if (len <= 0) {
                        continue
                    }
                    val index = audioCodec.dequeueInputBuffer(10_000)
                    if (index >= 0) {
                        //交给codec编码成aac
                        val buffer = audioCodec.getInputBuffer(index)
                        buffer?.also {
                            it.clear()
                            it.put(audioByte)
                        }
                        //通知dsp开始编码
                        audioCodec.queueInputBuffer(index, 0, len, System.nanoTime() / 1000, 0)
                    }

                    val bufferInfo = BufferInfo()
                    var outIndex = audioCodec.dequeueOutputBuffer(bufferInfo, 10_000)
                    while (outIndex >= 0) {
                        if (startTime == 0L) {
                            startTime = bufferInfo.presentationTimeUs / 1000
                        }
                        val outputBuffer = audioCodec.getOutputBuffer(outIndex)

                        val byteArray = ByteArray(bufferInfo.size)
                        outputBuffer?.get(byteArray)

                        val rtmpPackage = RTMPPackage(
                            byteArray,
                            bufferInfo.presentationTimeUs / 1000 - startTime,
                            RTMP_PACKET_TYPE_AUDIO
                        )
                        ConsumerLive.addPackage(rtmpPackage)
                        audioCodec.releaseOutputBuffer(outIndex, false)
                        outIndex = audioCodec.dequeueOutputBuffer(bufferInfo, 1_000)
                    }
                }
            }.onFailure {
                it.printStackTrace()
            }

        }
    }
}