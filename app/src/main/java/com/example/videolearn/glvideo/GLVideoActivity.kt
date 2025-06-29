package com.example.videolearn.glvideo

import android.graphics.Bitmap
import android.media.MediaFormat
import android.os.Bundle
import android.os.Looper
import android.util.Log
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.example.play.IPalyListener
import com.example.play.PlayManager
import com.example.play.TrackInterceptor
import com.example.videolearn.BaseActivity
import com.norman.android.hdrsample.handler.MessageHandler
import com.norman.android.hdrsample.handler.MessageHandler.LifeCycleCallback
import com.norman.android.hdrsample.opengl.GLEnvConfigSimpleChooser
import com.norman.android.hdrsample.opengl.GLEnvContextManager
import com.norman.android.hdrsample.opengl.GLEnvDisplay
import com.norman.android.hdrsample.opengl.GLTextureSurface
import com.norman.android.hdrsample.player.DecodePlayerImpl
import com.norman.android.hdrsample.player.GLRenderPboTarget
import com.norman.android.hdrsample.player.GLRenderTextureTarget
import com.norman.android.hdrsample.player.GLTextureY2YRenderer
import com.norman.android.hdrsample.player.GLVideoOutput.HdrBitDepth
import com.norman.android.hdrsample.player.GLVideoTransform
import com.norman.android.hdrsample.player.extract.VideoExtractor
import com.norman.android.hdrsample.player.source.LocalFileSource
import com.norman.android.hdrsample.transform.HDRToSDRVideoTransform
import com.norman.android.hdrsample.util.AppUtil
import com.norman.android.hdrsample.util.GLESUtil
import com.norman.android.hdrsample.util.LogUtils
import com.norman.android.hdrsample.util.LogUtils.i
import com.norman.android.hdrsample.util.MediaFormatUtil
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.suspendCancellableCoroutine
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.coroutines.resume

class GLVideoActivity : BaseActivity() {
    private val TAG = "GLVideoActivity"
    private val bitmap: MutableState<Bitmap?> = mutableStateOf(null)
    private lateinit var playManager: PlayManager

    /**
     * Y2Y纹理渲染
     */
    private val y2yExtTextureRenderer = GLTextureY2YRenderer()
    private lateinit var createLooper: Looper
    private lateinit var videoSurface: GLTextureSurface

    /**
     * Transform转换时用frontTarget和backTarget交替做为中转
     */
    private var frontTarget = GLRenderTextureTarget("frontTarget")

    private var backTarget = GLRenderTextureTarget("backTarget")

    private val pboTarget = GLRenderPboTarget()
    var colorRange: Int = 0
    var colorSpace: Int = 0

    private var maxContentLuminance = 0
    private var maxFrameAverageLuminance = 0
    private var maxMasteringLuminance = 0

    /**
     * 纹理处理转换器
     */
    private val transformList: MutableList<GLVideoTransform> = mutableListOf()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val path = intent.getStringExtra("filepath") ?: ""
        val extractor = VideoExtractor.create()
        extractor.setSource(LocalFileSource(path))
        transformList.add(HDRToSDRVideoTransform())

        val outputFormat = MediaFormat()
        MediaFormatUtil.setString(outputFormat, MediaFormat.KEY_MIME, extractor.mimeType)
        MediaFormatUtil.setInteger(outputFormat, MediaFormat.KEY_MAX_INPUT_SIZE, extractor.maxInputSize)
        MediaFormatUtil.setInteger(outputFormat, MediaFormat.KEY_PROFILE, extractor.profile)
        MediaFormatUtil.setInteger(outputFormat, MediaFormat.KEY_LEVEL, extractor.profileLevel)
        MediaFormatUtil.setByteBuffer(outputFormat, DecodePlayerImpl.KEY_CSD_0, extractor.csd0Buffer)
        MediaFormatUtil.setByteBuffer(outputFormat, DecodePlayerImpl.KEY_CSD_1, extractor.csd1Buffer)
        //MediaExtractor不兼容KEY_HDR10_PLUS_INFO，不论HDR10还是HDR10+出来的都是KEY_HDR_STATIC_INFO，后续看看怎么解决
        val hdrStaticInfo = MediaFormatUtil.getByteBuffer(outputFormat, MediaFormat.KEY_HDR_STATIC_INFO)
        if (hdrStaticInfo != null) {
            hdrStaticInfo.clear()
            hdrStaticInfo.position(1)
            hdrStaticInfo.limit(hdrStaticInfo.capacity())
            hdrStaticInfo.order(ByteOrder.LITTLE_ENDIAN)
            val shortBuffer = hdrStaticInfo.asShortBuffer()
            val primaryRChromaticityX = shortBuffer[0].toInt()
            val primaryRChromaticityY = shortBuffer[1].toInt()
            val primaryGChromaticityX = shortBuffer[2].toInt()
            val primaryGChromaticityY = shortBuffer[3].toInt()
            val primaryBChromaticityX = shortBuffer[4].toInt()
            val primaryBChromaticityY = shortBuffer[5].toInt()
            val whitePointChromaticityX = shortBuffer[6].toInt()
            val whitePointChromaticityY = shortBuffer[7].toInt()
            maxMasteringLuminance = shortBuffer[8].toInt()
            val minMasteringLuminance = shortBuffer[9].toInt()
            maxContentLuminance = shortBuffer[10].toInt()
            maxFrameAverageLuminance = shortBuffer[11].toInt()
            i(TAG, "onOutputFormatChanged hdrStaticInfo!=null")
        } else {
            i(TAG, "onOutputFormatChanged hdrStaticInfo==null")
            maxMasteringLuminance = 0
            maxContentLuminance = 0
            maxFrameAverageLuminance = 0
        }
        val videoWidth = extractor.width
        val videoHeight = extractor.height
        AppUtil.getAppContext().getExternalCacheDir()?.also {
            if (it.exists()) {
                it.delete()
            }
        }

        LogUtils.i(TAG, "path:${path} $videoWidth*$videoHeight")
        playManager = PlayManager().apply {
            postGLRunnable {
                val glEnvDisplay = GLEnvDisplay.createDisplay()


                // 8位
                val configRGBA8Bit = glEnvDisplay.chooseConfig(GLEnvConfigSimpleChooser.Builder()
                    .build())


                // 10位，注意alpha是2位，视频不需要alpha够用了，其他需要alpha的情况下就不够用了
                val configRGBA1010102Bit = glEnvDisplay.chooseConfig(GLEnvConfigSimpleChooser.Builder()
                    .setRedSize(10)
                    .setGreenSize(10)
                    .setBlueSize(10)
                    .setAlphaSize(2)
                    .build())

                //  16位
                val configRGBA16Bit = glEnvDisplay.chooseConfig(GLEnvConfigSimpleChooser.Builder()
                    .setRedSize(16)
                    .setGreenSize(16)
                    .setBlueSize(16)
                    .setAlphaSize(16)
                    .build())
                var envConfig = configRGBA8Bit
                GLEnvContextManager.create(glEnvDisplay, envConfig)
                    .attach()
                videoSurface = GLTextureSurface(GLESUtil.createExternalTextureId())
                videoSurface.setDefaultBufferSize(videoWidth, videoHeight)


                val profile10Bit = MediaFormatUtil.is10BitProfile(outputFormat)
                colorRange = MediaFormatUtil.getColorRange(outputFormat)
                colorSpace = MediaFormatUtil.getColorSpace(outputFormat)

                y2yExtTextureRenderer.setTextureId(videoSurface.textureId)
                y2yExtTextureRenderer.setBitDepth(8)
                y2yExtTextureRenderer.setColorRange(extractor.colorRange)

                y2yExtTextureRenderer.setBitDepth(if (profile10Bit) 10 else 8)
                y2yExtTextureRenderer.setColorRange(colorRange)
                y2yExtTextureRenderer.setRotation(extractor.rotation)
                init(object : IPalyListener {
                    override fun onVideoConfig(witdh: Int, height: Int, duration: Double, fps: Double, rotation: Int) {
                    }

                    override fun onPalyProgress(frame: ByteBuffer?, time: Double) {
                        LogUtils.i(TAG, "onPalyProgress time:${time}")
                        runBlocking {
                            suspendCancellableCoroutine<Unit> { continuation ->
                                postGLRunnable {
                                    videoSurface.updateTexImage()
                                    val textureMatrix: FloatArray = y2yExtTextureRenderer.getTextureMatrix()
                                    videoSurface.getTransformMatrix(textureMatrix) //纹理矩阵能解决绿边问题


                                    // 前面得到的纹理输出到frontTarget上

                                    // 前面得到的纹理输出到frontTarget上
                                    var targetBitDepth = 8
                                    frontTarget.setBitDepth(targetBitDepth)
                                    backTarget.setBitDepth(targetBitDepth)

                                    frontTarget.setRenderSize(videoWidth, videoHeight)
                                    backTarget.setRenderSize(videoWidth, videoHeight)
                                    pboTarget.setRenderSize(videoWidth, videoHeight)

                                    // 标记frontTarget的属性，方便后续处理

                                    // 标记frontTarget的属性，方便后续处理
                                    frontTarget.setColorSpace(colorSpace)
                                    frontTarget.setMaxContentLuminance(maxContentLuminance)
                                    frontTarget.setMaxFrameAverageLuminance(maxFrameAverageLuminance)
                                    frontTarget.setMaxMasteringLuminance(maxMasteringLuminance)
                                    // 标记frontTarget的属性，方便后续处理

                                    //把前面的数据渲染到新的纹理上面
                                    y2yExtTextureRenderer.renderToTarget(frontTarget)

                                    //用frontTarget和backTarget做中转做Transform的处理
                                    //这里单fbo也能处理,如果存在多层后处理的话,就需要用到双fbo

                                    //用frontTarget和backTarget做中转做Transform的处理
                                    //这里单fbo也能处理,如果存在多层后处理的话,就需要用到双fbo
                                    for (videoTransform in transformList) {
                                        //HDRToSDRVideoTransform
                                        //HDR->SDR
                                        videoTransform.renderToTarget(frontTarget, backTarget)
                                        val renderSuccess = videoTransform.renderSuccess
                                        i(TAG, " transformList for " + videoTransform.javaClass.simpleName + " frontTarget:" + frontTarget + " backTarget:" + backTarget + " success:" + renderSuccess)
                                        if (renderSuccess) { //如果绘制成功了，才中转纹理
                                            i(TAG, "transformList for $videoTransform  success")
                                            val temp = frontTarget
                                            frontTarget = backTarget
                                            backTarget = temp
                                        }
                                    }

                                    pboTarget.setTime((time * 100).toLong())
                                    frontTarget.startRender()
                                    pboTarget.startRender()
                                    pboTarget.finishRender()
                                    frontTarget.finishRender()
                                    continuation.resume(Unit)
                                }
                            }
                        }
                    }

                    override fun onPalyComplete() {
                    }

                    override fun onPlayError(code: Int) {
                    }

                })
                setTrackInterceptor(object : TrackInterceptor {
                    override fun onStart(duration: Double): DoubleArray {
                        val size = Math.ceil(duration).toInt() + 1
                        Log.i(TAG, "onStart duration:${duration} size:$size")
                        val ptsArrays = DoubleArray(size)
                        for (i in 0 until size) {
                            ptsArrays[i] = i.toDouble() * 1000
                        }
                        return ptsArrays
                    }

                })
                prepare(path, videoSurface)
                start()
            }
        }
        val itemSizeDP = 30.dp
        setContent {
            Image(painter = bitmap.value?.let {
                BitmapPainter(it.asImageBitmap())
            } ?: kotlin.run {
                ColorPainter(Color.Red)
            },
                contentDescription = "",
                modifier = Modifier

                    .width(itemSizeDP)
                    .height(itemSizeDP),
                contentScale = ContentScale.Crop)
        }
    }

    lateinit var messageHandler: MessageHandler
    fun postGLRunnable(runnable: Runnable) {
        if (!this::messageHandler.isInitialized) {
            messageHandler = MessageHandler.obtain("glvideo",
                object : LifeCycleCallback {
                    override fun onHandlerFinish() {
                        LogUtils.i(TAG, "onHandlerFinish")
                    }

                    override fun onHandlerError(exception: Exception) {
                        LogUtils.i(TAG, "onHandlerError ${exception.message}")
                    }
                })
        }
        messageHandler.post(runnable)
    }

    override fun onDestroy() {
        super.onDestroy()
        playManager.stop()
    }
}