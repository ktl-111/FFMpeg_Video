package com.example.videolearn.live

import android.content.Context
import android.graphics.ImageFormat
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Bundle
import android.util.Log
import android.util.Size
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.core.*
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import com.example.videolearn.utils.FileUtils
import com.example.videolearn.utils.ImgHelper
import java.util.concurrent.Executors

class LiveCameraEncoder(private val context: Context, private val previewView: PreviewView) {
    private val TAG = "LiveCameraEncoder"
    private var mediaCodec: MediaCodec? = null
    private val RATE = 15
    private var index = 0
    private fun initMedia(width: Int, height: Int) {
        if (mediaCodec != null) {
            return
        }
        val createVideoFormat =
            MediaFormat.createVideoFormat(
                MediaFormat.MIMETYPE_VIDEO_AVC,
                width,
                height
            )
        //帧率
        createVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, RATE)
        //多少帧一个I，非强制性
        createVideoFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2)
        //码率,越大越清晰
        createVideoFormat.setInteger(
            MediaFormat.KEY_BIT_RATE,
            width * height
        )
        //来源
        createVideoFormat.setInteger(
            MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
        )

        mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC).apply {
            configure(
                createVideoFormat,
                null,
                null,
                MediaCodec.CONFIGURE_FLAG_ENCODE
            )

            start()
        }

    }

    fun startCamera() {
        val instance = ProcessCameraProvider.getInstance(context)
        instance.addListener({
            val processCameraProvider = instance.get()

            val cameraSelector = CameraSelector.Builder()
                .requireLensFacing(CameraSelector.LENS_FACING_BACK)
                .build()

            val previewBuild = Preview.Builder()
                .setTargetAspectRatio(AspectRatio.RATIO_16_9)
                .build()

            previewBuild.setSurfaceProvider(previewView.surfaceProvider)

            val imageAnalysis = ImageAnalysis.Builder()
                .setTargetResolution(Size(1080, 1920))
                .setOutputImageRotationEnabled(true)
                .build()
            imageAnalysis.setAnalyzer(
                Executors.newSingleThreadExecutor()
            ) { image ->
                kotlin.runCatching {
                    require(image.format == ImageFormat.YUV_420_888) { "Invalid image format" }

//                    ImgHelper.useYuvImgSaveFile(image, true)
                    parseImage(image)

                }.onFailure {
                    Log.e(TAG, "startCamera: ERROR", it)
                }
                image.close()
            }

            processCameraProvider.unbindAll()
            processCameraProvider.bindToLifecycle(
                context as AppCompatActivity,
                cameraSelector,
                previewBuild, imageAnalysis
            )

        }, ContextCompat.getMainExecutor(context))
    }

    var startTime: Long = 0
    private fun parseImage(image: ImageProxy) {
        Log.i(TAG, "parseImage: ${image.width}*${image.height}")
        initMedia(image.width, image.height)
        val nv12Data = ImgHelper.getNV12ByteArray(image)
        val mediaCodec = mediaCodec!!


        val inputIndex = mediaCodec.dequeueInputBuffer(10_000)
        if (inputIndex >= 0) {

            //获取到buffer
            val inputBuffer = mediaCodec.getInputBuffer(inputIndex)
            inputBuffer?.also {
                it.clear()
                //设置数据
                it.put(nv12Data)
            }
            //通知dsp开始解码
            mediaCodec.queueInputBuffer(inputIndex, 0, nv12Data.size, computPts(), 0)
            index++
        }

        /*============输出===============*/
        val bufferInfo = MediaCodec.BufferInfo()
        //获取已编码的buffer index
        var outputIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
        while (outputIndex >= 0) {

            if (startTime == 0L) {
                startTime = bufferInfo.presentationTimeUs / 1000
            }
            //拿到对应buffer
            val outputBuffer = mediaCodec.getOutputBuffer(outputIndex)
            val byteArray = ByteArray(bufferInfo.size)
            //读取buffer
            outputBuffer?.get(byteArray)
            val name = "live_camera"
            FileUtils.writeBytes(byteArray, name)
//            FileUtils.writeContent(byteArray, name)


            val rtmpPackage = RTMPPackage(
                byteArray,
                bufferInfo.presentationTimeUs / 1000 - startTime,
                RTMP_PACKET_TYPE_VIDEO
            )

            ConsumerLive.addPackage(rtmpPackage)

            mediaCodec.releaseOutputBuffer(outputIndex, false)
            outputIndex = mediaCodec.dequeueOutputBuffer(bufferInfo, 10_000)
        }
    }

    private fun computPts(): Long {
        //微秒
        return (1_000_000).toLong() / RATE * index
    }
}