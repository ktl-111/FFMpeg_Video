// TextureViewRecorder.kt
package com.example.videolearn.recoder

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.SurfaceTexture
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.opengl.GLES20
import android.opengl.GLES30
import android.util.Log
import com.example.videolearn.App
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

class TextureViewRecorder(
    private val surfaceTexture: SurfaceTexture,
    private val width: Int,
    private val height: Int,
    private val sharedEglContext: EGLContext? = null  // 添加共享上下文参数
) {

    companion object {
        private const val TAG = "TextureViewRecorder"

        // 简单着色器（直通）
        private const val SIMPLE_VERTEX_SHADER = """
            attribute vec4 aPosition;
            attribute vec2 aTexCoord;
            varying vec2 vTexCoord;
            void main() {
                gl_Position = aPosition;
                vTexCoord = aTexCoord;
            }
        """

        private const val SIMPLE_FRAGMENT_SHADER = """
            precision mediump float;
            varying vec2 vTexCoord;
            uniform sampler2D uTexture;
            void main() {
                gl_FragColor = texture2D(uTexture, vTexCoord);
            }
        """
    }

    // EGL相关
    private var eglDisplay: EGLDisplay = EGL14.EGL_NO_DISPLAY
    private var eglContext: EGLContext = EGL14.EGL_NO_CONTEXT
    private var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE

    // OpenGL对象
    private var fboId = 0
    private var externalTextureId = 0
    private var fboTextureId = 0
    private val pboIds = IntArray(2)
    private var currentPboIndex = 0

    // 状态
    private var isRunning = false
    private var captureThread: Thread? = null

    // 着色器程序
    private var shaderProgram = 0
    private var positionHandle = 0
    private var texCoordHandle = 0
    private var textureHandle = 0
    private var matrixHandle = 0

    // 添加：用于读取数据的FBO纹理
    private var readFboId = 0
    private var readTextureId = 0

    fun start() {
        if (isRunning) {
            Log.w(TAG, "Already running")
            return
        }

        isRunning = true
        captureThread = Thread(this::captureLoop, "TextureView-Capture")
        captureThread?.start()
    }

    fun stop() {
        isRunning = false
        captureThread?.join(1000)
        captureThread = null
        release()
    }

    private fun captureLoop() {
        try {
            // 1. 初始化EGL环境（使用共享上下文）
            initEGL(sharedEglContext)

            // 2. 初始化OpenGL对象
            initOpenGL()

            Log.d(TAG, "Capture loop started")

            // 3. 捕获循环
            while (isRunning) {
                try {
                    // 等待新帧
                    Thread.sleep(33) // 约30fps

                    // 尝试从TextureView获取当前帧
                    // 注意：我们不能直接updateTexImage，因为TextureView在使用
                    // 我们需要使用glReadPixels从当前FBO读取
                    val frameData = captureCurrentFrame()

                    if (frameData != null) {
                        processFrameData(frameData)
                    }

                } catch (e: Exception) {
                    Log.e(TAG, "Error in capture loop", e)
                }
            }

        } catch (e: Exception) {
            Log.e(TAG, "Capture loop failed", e)
        } finally {
            release()
        }
    }

    private fun initEGL(sharedContext: EGLContext?) {
        // 1. 获取EGL Display
        eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
            throw RuntimeException("Unable to get EGL14 display")
        }

        // 2. 初始化EGL
        val version = IntArray(2)
        if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
            throw RuntimeException("Unable to initialize EGL14")
        }

        // 3. 选择配置
        val configAttribs = intArrayOf(
            EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
            EGL14.EGL_RED_SIZE, 8,
            EGL14.EGL_GREEN_SIZE, 8,
            EGL14.EGL_BLUE_SIZE, 8,
            EGL14.EGL_ALPHA_SIZE, 8,
            EGL14.EGL_DEPTH_SIZE, 0,
            EGL14.EGL_STENCIL_SIZE, 0,
            EGL14.EGL_NONE
        )

        val configs = arrayOfNulls<EGLConfig>(1)
        val numConfigs = IntArray(1)
        if (!EGL14.eglChooseConfig(
                eglDisplay, configAttribs, 0,
                configs, 0, configs.size, numConfigs, 0
            )
        ) {
            throw RuntimeException("Unable to choose EGL config")
        }
        val eglConfig = configs[0] ?: throw RuntimeException("No EGL config found")

        // 4. 创建上下文（使用共享上下文）
        val contextAttribs = intArrayOf(
            EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL14.EGL_NONE
        )

        eglContext = if (sharedContext != null) {
            // 使用共享上下文
            EGL14.eglCreateContext(
                eglDisplay, eglConfig,
                sharedContext, contextAttribs, 0
            )
        } else {
            // 创建新上下文
            EGL14.eglCreateContext(
                eglDisplay, eglConfig,
                EGL14.EGL_NO_CONTEXT, contextAttribs, 0
            )
        }

        if (eglContext == EGL14.EGL_NO_CONTEXT) {
            throw RuntimeException("Failed to create EGL context")
        }

        // 5. 创建Pbuffer表面（离屏渲染，不绑定到TextureView的Surface）
        // 注意：我们不能使用TextureView的SurfaceTexture创建窗口表面
        val surfaceAttribs = intArrayOf(
            EGL14.EGL_WIDTH, width,
            EGL14.EGL_HEIGHT, height,
            EGL14.EGL_NONE
        )

        eglSurface = EGL14.eglCreatePbufferSurface(eglDisplay, eglConfig, surfaceAttribs, 0)

        if (eglSurface == EGL14.EGL_NO_SURFACE) {
            throw RuntimeException("Failed to create Pbuffer surface")
        }

        // 6. 设置为当前上下文
        if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            throw RuntimeException("Failed to make EGL context current")
        }

        Log.d(TAG, "EGL initialized successfully with Pbuffer surface")
    }

    private fun initOpenGL() {
        // 1. 创建用于读取的FBO
        readFboId = createFBOForReading()

        // 2. 创建着色器程序（简单直通着色器）
        shaderProgram = createProgram(SIMPLE_VERTEX_SHADER, SIMPLE_FRAGMENT_SHADER)
        positionHandle = GLES20.glGetAttribLocation(shaderProgram, "aPosition")
        texCoordHandle = GLES20.glGetAttribLocation(shaderProgram, "aTexCoord")

        // 3. 创建PBOs（双缓冲）
        GLES30.glGenBuffers(2, pboIds, 0)
        for (i in 0 until 2) {
            GLES30.glBindBuffer(GLES30.GL_PIXEL_PACK_BUFFER, pboIds[i])
            GLES30.glBufferData(
                GLES30.GL_PIXEL_PACK_BUFFER,
                width * height * 4,
                null,
                GLES30.GL_STREAM_READ
            )
        }
        GLES30.glBindBuffer(GLES30.GL_PIXEL_PACK_BUFFER, 0)

        Log.d(TAG, "OpenGL initialized successfully")
    }

    private fun createFBOForReading(): Int {
        // 创建FBO
        val fbos = IntArray(1)
        GLES20.glGenFramebuffers(1, fbos, 0)
        val fboId = fbos[0]

        // 创建纹理
        val textures = IntArray(1)
        GLES20.glGenTextures(1, textures, 0)
        readTextureId = textures[0]

        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, readTextureId)
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)

        // 分配纹理内存
        GLES20.glTexImage2D(
            GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA,
            width, height, 0, GLES20.GL_RGBA,
            GLES20.GL_UNSIGNED_BYTE, null
        )

        // 绑定纹理到FBO
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fboId)
        GLES20.glFramebufferTexture2D(
            GLES20.GL_FRAMEBUFFER,
            GLES20.GL_COLOR_ATTACHMENT0,
            GLES20.GL_TEXTURE_2D,
            readTextureId,
            0
        )

        // 检查FBO完整性
        val status = GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER)
        if (status != GLES20.GL_FRAMEBUFFER_COMPLETE) {
            throw RuntimeException("Framebuffer not complete: $status")
        }

        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0)

        return fboId
    }

    private fun captureCurrentFrame(): ByteBuffer? {
        // 注意：这个方法不能直接调用，因为TextureView的帧在其自己的GL上下文中
        // 我们需要一个不同的方法：使用glReadPixels从当前绑定的FBO读取

        // 绑定读取FBO
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, readFboId)

        // 设置视口
        GLES20.glViewport(0, 0, width, height)

        // 清除颜色
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f)
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)

        // 这里我们无法直接获取TextureView的内容
        // 需要另一种方法：使用PixelCopy或直接读取纹理数据

        return readFromPBO()
    }

    private fun readFromPBO(): ByteBuffer? {
        // 切换到下一个PBO
        currentPboIndex = (currentPboIndex + 1) % 2
        val nextPboIndex = (currentPboIndex + 1) % 2

        // 绑定当前PBO（用于读取）
        GLES30.glBindBuffer(GLES30.GL_PIXEL_PACK_BUFFER, pboIds[currentPboIndex])

        // 从当前FBO读取像素到PBO（异步）
        GLES30.glReadPixels(
            0, 0, width, height,
            GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, 0
        )

        // 绑定下一个PBO（用于映射）
        GLES30.glBindBuffer(GLES30.GL_PIXEL_PACK_BUFFER, pboIds[nextPboIndex])

        // 映射PBO到客户端内存
        val buffer = GLES30.glMapBufferRange(
            GLES30.GL_PIXEL_PACK_BUFFER, 0, width * height * 4,
            GLES30.GL_MAP_READ_BIT
        ) as? ByteBuffer

//        var frameData: ByteArray? = null
//        if (buffer != null) {
//            // 复制数据
//            frameData = ByteArray(buffer.remaining())
//            buffer.get(frameData)
//
//            // 解映射
//            GLES30.glUnmapBuffer(GLES30.GL_PIXEL_PACK_BUFFER)
//        }

        // 解绑PBO
        GLES30.glBindBuffer(GLES30.GL_PIXEL_PACK_BUFFER, 0)

        // 解绑FBO
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0)

        return buffer
    }

    // 这个方法用于外部传入帧数据（从TextureView获取）
    fun processExternalFrame(bitmapData: ByteArray) {
        // 将外部数据上传到纹理
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, readTextureId)
        GLES20.glTexSubImage2D(
            GLES20.GL_TEXTURE_2D, 0, 0, 0,
            width, height, GLES20.GL_RGBA,
            GLES20.GL_UNSIGNED_BYTE,
            ByteBuffer.wrap(bitmapData)
        )

        // 现在可以通过PBO读取
        val frameData = readFromPBO()
        if (frameData != null) {
            processFrameData(frameData)
        }
    }

    private fun processFrameData(frameData: ByteBuffer) {
        // 这里处理帧数据，例如编码为视频
        saveBufferToLocal(frameData)
    }

    private fun saveBufferToLocal(buffer: ByteBuffer) {
        buffer.rewind()
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        bitmap.copyPixelsFromBuffer(buffer)
        saveBitmap(bitmap) // 保存到本地
        bitmap.recycle()
    }

    // 保存到本地方法
    private fun saveBitmap(bitmap: Bitmap) {
        val path: String = File(App.application?.getExternalCacheDir(), "${System.currentTimeMillis()}.png").getAbsolutePath()
        try {
            FileOutputStream(path).use { out ->
                bitmap.compress(Bitmap.CompressFormat.JPEG, 100, out)
                Log.i(TAG, "save bitmap to $path success")
            }
        } catch (e: IOException) {
            Log.e(TAG, "Save failed " + e.message)
        }
    }


    private fun createProgram(vertexSource: String, fragmentSource: String): Int {
        val vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexSource)
        val fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource)

        val program = GLES20.glCreateProgram()
        GLES20.glAttachShader(program, vertexShader)
        GLES20.glAttachShader(program, fragmentShader)
        GLES20.glLinkProgram(program)

        val linkStatus = IntArray(1)
        GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0)
        if (linkStatus[0] != GLES20.GL_TRUE) {
            val error = GLES20.glGetProgramInfoLog(program)
            GLES20.glDeleteProgram(program)
            throw RuntimeException("Program link failed: $error")
        }

        return program
    }

    private fun loadShader(type: Int, shaderCode: String): Int {
        val shader = GLES20.glCreateShader(type)
        GLES20.glShaderSource(shader, shaderCode)
        GLES20.glCompileShader(shader)

        val compiled = IntArray(1)
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0)
        if (compiled[0] == 0) {
            val error = GLES20.glGetShaderInfoLog(shader)
            GLES20.glDeleteShader(shader)
            throw RuntimeException("Shader compilation failed: $error")
        }

        return shader
    }

    private fun release() {
        // 释放OpenGL资源
        if (readTextureId != 0) {
            val textures = intArrayOf(readTextureId)
            GLES20.glDeleteTextures(1, textures, 0)
            readTextureId = 0
        }

        if (readFboId != 0) {
            val fbos = intArrayOf(readFboId)
            GLES20.glDeleteFramebuffers(1, fbos, 0)
            readFboId = 0
        }

        if (pboIds[0] != 0) {
            GLES30.glDeleteBuffers(2, pboIds, 0)
            pboIds[0] = 0
            pboIds[1] = 0
        }

        if (shaderProgram != 0) {
            GLES20.glDeleteProgram(shaderProgram)
            shaderProgram = 0
        }

        // 释放EGL资源
        if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
            EGL14.eglMakeCurrent(
                eglDisplay,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_CONTEXT
            )

            if (eglSurface != EGL14.EGL_NO_SURFACE) {
                EGL14.eglDestroySurface(eglDisplay, eglSurface)
            }

            if (eglContext != EGL14.EGL_NO_CONTEXT) {
                EGL14.eglDestroyContext(eglDisplay, eglContext)
            }

            EGL14.eglTerminate(eglDisplay)
        }

        eglDisplay = EGL14.EGL_NO_DISPLAY
        eglContext = EGL14.EGL_NO_CONTEXT
        eglSurface = EGL14.EGL_NO_SURFACE

        Log.d(TAG, "Released all resources")
    }

}