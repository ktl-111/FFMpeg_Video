package com.example.videolearn.videoplay

import android.media.*
import android.util.Log
import android.view.Surface
import com.example.videolearn.MediaScope
import com.example.videolearn.utils.FileUtils
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch


class VideoPlayHelper(private val path: String, private val surface: Surface) {
    private val START_VIDEO = "video/"
    private val START_AUDIO = "audio/"
    private val TAG = "VideoPlayHelper"
    var audioTrack: AudioTrack? = null
    var audioCodec: MediaCodec? = null
    var audioExtractor: MediaExtractor? = null

    var videoCodec: MediaCodec? = null
    var videoExtractor: MediaExtractor? = null
    var startTime = 0L
    fun play() {
        startTime = System.currentTimeMillis()
        MediaScope.launch {
            parseVideo()
        }
        MediaScope.launch {
            parseAudio()
        }
    }

    private suspend fun parseVideo() {
        //创建音视频分离器
        videoExtractor = MediaExtractor()
        val videoExtractor = videoExtractor!!
        videoExtractor.setDataSource(path)
        //获取video轨道index
        val videoTrackIndex = getTrackIndex(videoExtractor, START_VIDEO)
        val index = videoTrackIndex.first
        if (index != -1) {
            //获取fromat,内部就是读取sps和pps信息
            val trackFormat = videoExtractor.getTrackFormat(index)
            val width = trackFormat.getInteger(MediaFormat.KEY_WIDTH)
            val height = trackFormat.getInteger(MediaFormat.KEY_HEIGHT)
            val second = trackFormat.getLong(MediaFormat.KEY_DURATION) / 1000_000
            Log.i(TAG, "parseVideo width:$width,height:$height,second:$second")


            //选择轨道
            videoExtractor.selectTrack(index)

            //创建视频解码
            videoCodec = MediaCodec.createDecoderByType(videoTrackIndex.second)
            val videoCodec = videoCodec!!
            videoCodec.configure(trackFormat, surface, null, 0)
            videoCodec.start()

            while (true) {
                decodeMediaData(videoExtractor, videoCodec)

                val bufferInfo = MediaCodec.BufferInfo()
                //获取是否解码完成index
                var dequeueOutputBuffer = videoCodec.dequeueOutputBuffer(bufferInfo, 10_000)
                //阻塞回调,一直等待解码完成
                while (dequeueOutputBuffer >= 0) {
                    videoCodec.getOutputBuffer(dequeueOutputBuffer)?.also {
                        val byteArray = ByteArray(bufferInfo.size)
                        it.get(byteArray)
                        // TODO: 非h264数据?????
                        FileUtils.writeContent(byteArray, "VideoPlay_video")
                    }
                    decodeDelay("video", bufferInfo, startTime)
                    videoCodec.releaseOutputBuffer(dequeueOutputBuffer, true)
                    dequeueOutputBuffer = videoCodec.dequeueOutputBuffer(bufferInfo, 0)
                }

                //读到结束
                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM !== 0) {
                    break
                }
            }
            releaseVideo()
        }
    }

    private suspend fun decodeDelay(
        tag: String,
        bufferInfo: MediaCodec.BufferInfo,
        startMillis: Long
    ) {
        Log.i(
            TAG,
            "decodeDelay $tag: ${bufferInfo.presentationTimeUs / 1000} ${System.currentTimeMillis() - startMillis}"
        )
        do {
            //bufferInfo.presentationTimeUs 当前读取数据的pts
            //需要check当前时间是否在pts内,否则需要等到pts之后才解析下一帧
            val diff =
                bufferInfo.presentationTimeUs / 1000 - (System.currentTimeMillis() - startMillis)
            if (diff > 0) {
                delay(diff)
            }
        } while (diff > 0)
    }

    private fun releaseVideo() {
        // 释放解码器
        videoCodec?.apply {
            stop()
            release()
        }
        videoExtractor?.release()
    }

    private suspend fun parseAudio() {
        audioExtractor = MediaExtractor()
        val audioExtractor = audioExtractor!!
        audioExtractor.setDataSource(path)
        val trackIndex = getTrackIndex(audioExtractor, START_AUDIO)
        val audioTrackIndex = trackIndex.first
        if (audioTrackIndex != -1) {
            //选择轨道
            audioExtractor.selectTrack(audioTrackIndex)

            val trackFormat = audioExtractor.getTrackFormat(audioTrackIndex)
            //采样率
            val sampleRate = trackFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE)
            //最大size
            val maxInputSize = trackFormat.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE)
            //通道数
            val channelCount = trackFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT)

            Log.i(
                TAG,
                "parseAudio sampleRate:${sampleRate} maxInputSize:${maxInputSize} channelCount:${channelCount}"
            )
            //创建音乐播放器
            val minBufferSize = AudioTrack.getMinBufferSize(
                sampleRate,
                if (channelCount == 1) AudioFormat.CHANNEL_OUT_MONO else AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT
            )
            var mInputBufferSize = if (minBufferSize > 0) minBufferSize * 4 else maxInputSize
            audioTrack = AudioTrack(
                AudioManager.STREAM_MUSIC,
                sampleRate,
                if (channelCount === 1) AudioFormat.CHANNEL_OUT_MONO else AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT,
                mInputBufferSize,
                AudioTrack.MODE_STREAM
            )
            val audioTrack = audioTrack!!
            audioTrack.play()

            //创建音频解码
            audioCodec = MediaCodec.createDecoderByType(trackIndex.second)
            val audioCodec = audioCodec!!
            audioCodec.configure(trackFormat, null, null, 0)
            audioCodec.start()

            var audioDataArray: ByteArray

            while (true) {
                decodeMediaData(audioExtractor, audioCodec)

                val bufferInfo = MediaCodec.BufferInfo()
                //获取是否解码完成index
                var dequeueOutputBuffer = audioCodec.dequeueOutputBuffer(bufferInfo, 10_000)
                //阻塞回调,一直等待解码完成
                while (dequeueOutputBuffer >= 0) {
                    audioCodec.getOutputBuffer(dequeueOutputBuffer)?.also {
                        audioDataArray = ByteArray(bufferInfo.size)
                        it.get(audioDataArray)
                        FileUtils.writeContent(audioDataArray, "VideoPlay_audio")
                        audioTrack.write(audioDataArray, 0, audioDataArray.size)
                    }
//                    decodeDelay("audio", bufferInfo, startTime)
                    audioCodec.releaseOutputBuffer(dequeueOutputBuffer, true)
                    dequeueOutputBuffer = audioCodec.dequeueOutputBuffer(bufferInfo, 0)
                }

                //读到结束
                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM !== 0) {
                    break
                }
            }

            releaseAudio()
        }
    }

    private fun releaseAudio() {
        audioCodec?.apply {
            stop()
            release()
        }
        audioTrack?.apply {
            stop()
            release()
        }
        audioExtractor?.release()
    }

    private fun getTrackIndex(
        mediaExtractor: MediaExtractor,
        startWith: String
    ): Pair<Int, String> {
        val trackCount = mediaExtractor.trackCount
        for (index in 0 until trackCount) {
            val trackFormat = mediaExtractor.getTrackFormat(index)
            trackFormat.getString(MediaFormat.KEY_MIME)?.also {
                if (it.startsWith(startWith)) {
                    return index to it
                }
            }
        }
        return -1 to ""
    }

    val TIMEOUT_US = 10_000L

    /**
     * 解析video数据
     */
    private fun decodeMediaData(
        extractor: MediaExtractor,
        decoder: MediaCodec
    ) {
        //获取空闲的index
        val inputBufferIndex = decoder.dequeueInputBuffer(TIMEOUT_US)
        if (inputBufferIndex >= 0) {
            decoder.getInputBuffer(inputBufferIndex)?.also { inputBuffer ->
                //extractor将数据解析到buff
                val sampleSize = extractor.readSampleData(inputBuffer, 0)
                if (sampleSize < 0) {
                    //通知已经读完
                    decoder.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        0,
                        0,
                        //结束flag
                        MediaCodec.BUFFER_FLAG_END_OF_STREAM
                    )
                } else {
                    //通知编码
                    decoder.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        sampleSize,
                        extractor.sampleTime,
                        extractor.sampleFlags
                    )
                    //游标移到下一帧,如果没有数据,会返回false
                    extractor.advance()
                }
            }

        }
    }

    fun release() {
        releaseVideo()
        releaseAudio()
    }
}