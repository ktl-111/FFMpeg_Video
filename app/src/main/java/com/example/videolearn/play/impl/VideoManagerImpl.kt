package com.example.videolearn.play.impl

import android.media.MediaFormat
import com.example.play.IPlayListener
import com.example.play.PlayManager
import com.example.play.TrackInterceptor
import com.example.play.utils.DecodeUtils
import com.example.play.utils.FFMpegUtils
import com.example.videolearn.play.Operate
import com.example.videolearn.play.PlaybackControlApi
import com.norman.android.hdrsample.opengl.GLEnvConfigSimpleChooser
import com.norman.android.hdrsample.opengl.GLEnvContextManager
import com.norman.android.hdrsample.opengl.GLEnvDisplay
import com.norman.android.hdrsample.opengl.GLTextureSurface
import com.norman.android.hdrsample.player.GLRenderPboTarget
import com.norman.android.hdrsample.player.GLRenderScreenTarget
import com.norman.android.hdrsample.player.GLRenderTextureTarget
import com.norman.android.hdrsample.player.GLTexture2DRenderer
import com.norman.android.hdrsample.player.GLTextureOESRenderer
import com.norman.android.hdrsample.player.GLTextureY2YRenderer
import com.norman.android.hdrsample.player.GLVideoTransform
import com.norman.android.hdrsample.player.OutputSurface
import com.norman.android.hdrsample.player.color.ColorSpace
import com.norman.android.hdrsample.transform.HDRToSDRVideoTransform
import com.norman.android.hdrsample.transform.shader.chromacorrect.ChromaCorrection
import com.norman.android.hdrsample.transform.shader.gamma.GammaOETF
import com.norman.android.hdrsample.transform.shader.gamutmap.GamutMap
import com.norman.android.hdrsample.transform.shader.tonemap.ToneMap
import com.norman.android.hdrsample.util.AppUtil
import com.norman.android.hdrsample.util.GLESUtil
import com.norman.android.hdrsample.util.LogUtils
import com.norman.android.hdrsample.util.MediaFormatUtil
import com.norman.android.hdrsample.util.TimeUtil
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.runBlocking
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors

open class VideoManagerImpl<T : Operate>(protected val path: String, protected val operate: T, protected val videoFormat: MediaFormat) : PlaybackControlApi {
    protected val TAG = "VideoManagerImpl"
    protected val mediaScope: CoroutineScope = CoroutineScope(Executors.newSingleThreadExecutor { runnable ->
        val t = Thread(runnable)
        t.name = "video_$operate"
        if (t.isDaemon) t.isDaemon = false
        if (t.priority != Thread.NORM_PRIORITY) t.priority = Thread.NORM_PRIORITY
        t
    }.asCoroutineDispatcher() + CoroutineExceptionHandler { coroutineContext, throwable ->
        LogUtils.e(TAG, "CoroutineExceptionHandler ${throwable.message}")
    })
    protected val playManager: PlayManager

    init {
        playManager = initPlayManager(path, operate)
    }


    override fun start() {
        LogUtils.i(TAG, "start ${operate}")
        if (operate is Operate.EditingOperate) {
            operate.onIs<Operate.EditingOperate> { operate ->
                DecodeUtils.startDecode(path, destPath = operate.destPath, startTime = operate.startTime, endTime = operate.startTime + operate.allTime, config = operate.outConfig, object : FFMpegUtils.VideoCuttingInterface {
                    override fun onStart() {
                        operate.editingCallback.onStart()
                    }

                    override fun onProgress(progress: Double) {
                        operate.editingCallback.onEditingProgress(progress)
                    }

                    override fun onFail(resultCode: Int) {
                        operate.editingCallback.onFail(resultCode)
                    }

                    override fun onDone() {
                        operate.editingCallback.onEditingDone()
                    }
                })
                playManager.start()
            }
        } else {
            playManager.start()
        }
    }

    override fun stop() {
        LogUtils.i(TAG, "stop ${operate}")
        playManager.stop()
    }

    override fun resume() {
        LogUtils.i(TAG, "resume ${operate}")
        playManager.resume()
    }

    override fun pause() {
        LogUtils.i(TAG, "pause ${operate}")
        playManager.pause()
    }


    private inline fun <reified T : Operate> Operate.onIs(action: (T) -> Unit) {
        operate.taskIfIs<T>()?.let { action(it) }
    }

    private inline fun <reified T : Operate> Operate.taskIfIs(): T? = this as? T


    private fun initPlayManager(path: String, operate: Operate): PlayManager {
        LogUtils.i(TAG, "initPlayManager START operate:${operate} videoFormat:${videoFormat} ")
        //MediaExtractor不兼容KEY_HDR10_PLUS_INFO，不论HDR10还是HDR10+出来的都是KEY_HDR_STATIC_INFO，后续看看怎么解决
        val hdrStaticInfo = MediaFormatUtil.getByteBuffer(videoFormat, MediaFormat.KEY_HDR_STATIC_INFO)
        val maxMasteringLuminance: Int
        val maxContentLuminance: Int
        val maxFrameAverageLuminance: Int
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
            LogUtils.i(TAG, "onOutputFormatChanged hdrStaticInfo!=null")
        } else {
            LogUtils.i(TAG, "onOutputFormatChanged hdrStaticInfo==null")
            maxMasteringLuminance = 0
            maxContentLuminance = 0
            maxFrameAverageLuminance = 0
        }
        var videoWidth = MediaFormatUtil.getInteger(videoFormat, MediaFormat.KEY_WIDTH)
        var videoHeight = MediaFormatUtil.getInteger(videoFormat, MediaFormat.KEY_HEIGHT)
        val rotation = MediaFormatUtil.getInteger(videoFormat, MediaFormat.KEY_ROTATION)
        if (rotation == 90 || rotation == 270) {
            val (x, y) = arrayOf(videoWidth, videoHeight)
            videoWidth = y
            videoHeight = x
        }
        videoWidth = (videoWidth * operate.outConfig.scale).toInt()
        videoHeight = (videoHeight * operate.outConfig.scale).toInt()
        val duration = MediaFormatUtil.getLong(videoFormat, MediaFormat.KEY_DURATION) / 1000 / 1000
        AppUtil.getAppContext().externalCacheDir?.also {
            val delete = it.deleteRecursively()
            LogUtils.i(TAG, "delete ${it.absolutePath} result:${delete}")
        }
        LogUtils.i(TAG, "path:${path} $videoWidth*$videoHeight ${duration} ${operate}")
        return PlayManager().apply {
            runBlocking(mediaScope.coroutineContext) {
                val transformList: MutableList<GLVideoTransform> = mutableListOf()
                transformList.add(HDRToSDRVideoTransform().apply {
                    setGammaOETF(GammaOETF.BT1886)
                    setGamutMap(GamutMap.CLIP)
                    setToneMap(ToneMap.ANDROID13)
                    setChromaCorrection(ChromaCorrection.BT2446C)
                    enable()
                })
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

                val profile10Bit = MediaFormatUtil.is10BitProfile(videoFormat)

                var glEnvConfig = if (profile10Bit) {
                    configRGBA1010102Bit
                } else {
                    configRGBA8Bit
                }
                if (glEnvConfig == null) {
                    glEnvConfig = configRGBA8Bit
                }
                val glEnvContextManager = GLEnvContextManager.create(glEnvDisplay, glEnvConfig)
                glEnvContextManager.attach()
                val envContext = glEnvContextManager.envContext
                val outputSurface = OutputSurface(envContext)
                if (operate is Operate.PlayOperate) {
                    outputSurface.setSurface(operate.surface)
                }

                val colorRange = MediaFormatUtil.getColorRange(videoFormat)
                val colorSpace = MediaFormatUtil.getColorSpace(videoFormat)

                /**
                 * Transform转换时用frontTarget和backTarget交替做为中转
                 */
                var frontTarget = GLRenderTextureTarget("frontTarget")

                var backTarget = GLRenderTextureTarget("backTarget")

                val pboTarget = GLRenderPboTarget()

                val texture2DRenderer = GLTexture2DRenderer()
                val screenTarget = GLRenderScreenTarget()

                val videoSurface = GLTextureSurface(GLESUtil.createExternalTextureId(operate.getOperateSize(), operate.textureIndex))
                videoSurface.setDefaultBufferSize(videoWidth, videoHeight)
                val textureY2YMode = colorSpace != ColorSpace.VIDEO_SDR && GLTextureY2YRenderer.isSupportY2YEXT()
                val glTextureRenderer = if (textureY2YMode) GLTextureY2YRenderer() else GLTextureOESRenderer()
                glTextureRenderer.setTextureId(videoSurface.textureId)
                val bitdepth = if (profile10Bit) 10 else 8
                if (glTextureRenderer is GLTextureY2YRenderer) {
                    glTextureRenderer.setBitDepth(bitdepth)
                    glTextureRenderer.setColorRange(colorRange)
                }
                videoSurface.setOnFrameAvailableListener {

                }
                glTextureRenderer.setRotation(MediaFormatUtil.getInteger(videoFormat, MediaFormat.KEY_ROTATION))
                init(object : IPlayListener {
                    override fun onVideoConfig(witdh: Int, height: Int, duration: Double, fps: Double, rotation: Int) {
                    }

                    override fun onPlayProgress(frame: ByteBuffer?, time: Double) {
                        LogUtils.i(TAG, "onPlayProgress time:${time}")
                        runBlocking(mediaScope.coroutineContext) {
                            LogUtils.i(TAG, "onPlayProgress time:${time}")
                            if (updateProgressPreCheck(time.toLong())) {
                                return@runBlocking
                            }

                            LogUtils.i(TAG, "start gl parse operate:${operate}")
                            videoSurface.updateTexImage()
                            val textureMatrix: FloatArray = glTextureRenderer.getTextureMatrix()
                            videoSurface.getTransformMatrix(textureMatrix) //纹理矩阵能解决绿边问题
                            frontTarget.setBitDepth(bitdepth)
                            backTarget.setBitDepth(bitdepth)
                            // 前面得到的纹理输出到frontTarget上

                            frontTarget.setRenderSize(videoWidth, videoHeight)
                            backTarget.setRenderSize(videoWidth, videoHeight)
                            pboTarget.setRenderSize(videoWidth, videoHeight)
                            // 标记frontTarget的属性，方便后续处理
                            frontTarget.setColorSpace(colorSpace)
                            frontTarget.setMaxContentLuminance(maxContentLuminance)
                            frontTarget.setMaxFrameAverageLuminance(maxFrameAverageLuminance)
                            frontTarget.setMaxMasteringLuminance(maxMasteringLuminance)
                            //把前面的数据渲染到新的纹理上面
                            glTextureRenderer.renderToTarget(frontTarget)

                            //用frontTarget和backTarget做中转做Transform的处理
                            //这里单fbo也能处理,如果存在多层后处理的话,就需要用到双fbo
                            for (videoTransform in transformList) {
                                //HDRToSDRVideoTransform
                                //HDR->SDR
                                videoTransform.renderToTarget(frontTarget, backTarget)
                                val renderSuccess = videoTransform.renderSuccess
                                LogUtils.i(TAG, " transformList for " + videoTransform.javaClass.simpleName + " frontTarget:" + frontTarget + " backTarget:" + backTarget + " success:" + renderSuccess)
                                if (renderSuccess) { //如果绘制成功了，才中转纹理
                                    LogUtils.i(TAG, "transformList for $videoTransform  success")
                                    val temp = frontTarget
                                    frontTarget = backTarget
                                    backTarget = temp
                                }
                            }
                            val timeUs = (time * 1000).toLong()
                            when (operate) {
                                is Operate.PlayOperate -> {
                                    operate.playCallback.onPlayProgress(time.toLong())
                                    // 获得最终纹理
                                    texture2DRenderer.setTextureId(frontTarget.textureId)

                                    val windowSurface = outputSurface.getWindowSurface(frontTarget.colorSpace)
                                    if (windowSurface != null) {
                                        envContext.makeCurrent(windowSurface)
                                        LogUtils.i(TAG, "onOutputBufferRender: windowSurface:$windowSurface")
                                        screenTarget.setRenderSize(windowSurface.width, windowSurface.height)
                                        screenTarget.clearColor()
                                        texture2DRenderer.renderToTarget(screenTarget)
                                        windowSurface.setPresentationTime(TimeUtil.microToNano(timeUs.toLong()))
                                        windowSurface.swapBuffers()
                                    }
                                }

                                is Operate.TrackOperate, is Operate.EditingOperate -> {
                                    pboTarget.setTime(timeUs)
                                    pboTarget.setBufferCallback { buffer ->
                                        updateProgressFrame(buffer, videoWidth, videoHeight, time.toLong())
                                    }
                                    frontTarget.startRender()
                                    pboTarget.startRender()
                                    pboTarget.finishRender()
                                    frontTarget.finishRender()
                                }
                            }
                        }
                    }

                    override fun onPlayComplete() {
                        if (operate is Operate.PlayOperate) {
                            operate.playCallback.onPlayComplete()
                        }
                    }

                    override fun onPlayError(code: Int) {
                        LogUtils.i(TAG, "onPlayError:${code}")
                    }

                })
                if (operate is Operate.TrackOperate) {
                    setTrackInterceptor(object : TrackInterceptor {
                        override fun onStart(duration: Double): LongArray {
                            return operate.videoTrackCallback.videoTrackInterval(duration.toLong())
                        }

                    })
                }

                prepare(path, videoSurface, outConfig = operate.outConfig)
            }
        }
    }

    protected open fun updateProgressPreCheck(time: Long): Boolean = false
    protected open fun updateProgressFrame(buffer: ByteBuffer, width: Int, height: Int, time: Long) {}

}