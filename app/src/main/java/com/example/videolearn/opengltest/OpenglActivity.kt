package com.example.videolearn.opengltest

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.opengl.GLUtils
import android.os.Bundle
import android.view.ViewGroup
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.viewinterop.AndroidView
import com.example.videolearn.BaseActivity
import com.example.videolearn.R
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class OpenglActivity : BaseActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
//        setContent {
//            RootView()
//        }
        val glSurfaceView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(2)  // 使用OpenGL ES 2.0
            setRenderer(BitmapRenderer(resources))
            renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
        }
        setContentView(glSurfaceView)
    }

    // 自定义渲染器
    class BitmapRenderer(private val res: Resources) : GLSurfaceView.Renderer {
        private lateinit var bitmap: Bitmap
        private val vertices = floatArrayOf(
            // 顶点坐标 (x,y) + 纹理坐标 (s,t)
            -1f, -1f, 0f, 1f,  //顶点(左下)->纹理(左上)
            -1f, 1f, 0f, 0f,  //顶点(左上)->纹理(左下)
            1f, -1f, 1f, 1f,   //顶点(右下)->纹理(右上)
            1f, 1f, 1f, 0f, //顶点(右上)->纹理(右下)
        )
        private var program = 0
        private var textureId = 0

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            // 1. 加载着色器
            val vertexShader = loadShader(
                GLES20.GL_VERTEX_SHADER,
                "attribute vec4 vPosition; " +
                        "attribute vec2 vTexCoord; " +
                        "varying vec2 texCoord; " +
                        "void main() { " +
                        "gl_Position = vPosition; " +
                        "texCoord = vTexCoord; }"
            )
            val fragmentShader = loadShader(
                GLES20.GL_FRAGMENT_SHADER,
                "precision mediump float; " +
                        "varying vec2 texCoord; " +
                        "uniform sampler2D texture; " +
                        "void main() { " +
                        "gl_FragColor = texture2D(texture, texCoord); }"
            )

            // 2. 创建OpenGL程序
            program = GLES20.glCreateProgram().also {
                GLES20.glAttachShader(it, vertexShader)
                GLES20.glAttachShader(it, fragmentShader)
                GLES20.glLinkProgram(it)
            }

            // 3. 加载Bitmap并创建纹理
            bitmap = BitmapFactory.decodeResource(res, R.drawable.ic_test)
            textureId = createTexture(bitmap)
        }

        private fun createTexture(bitmap: Bitmap): Int {
            val textureIds = IntArray(1)
            GLES20.glGenTextures(1, textureIds, 0)
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureIds[0])

            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)

            GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0)
            return textureIds[0]
        }

        override fun onDrawFrame(gl: GL10?) {
            GLES20.glClearColor(0f, 0f, 0f, 1f)
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)

            GLES20.glUseProgram(program)
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId)

            val vertexBuffer = ByteBuffer.allocateDirect(vertices.size * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(vertices).apply { position(0) }

            val positionHandle = GLES20.glGetAttribLocation(program, "vPosition")
            GLES20.glEnableVertexAttribArray(positionHandle)
            GLES20.glVertexAttribPointer(positionHandle, 2, GLES20.GL_FLOAT, false, 16, vertexBuffer)

            val texCoordHandle = GLES20.glGetAttribLocation(program, "vTexCoord")
            GLES20.glEnableVertexAttribArray(texCoordHandle)
            GLES20.glVertexAttribPointer(texCoordHandle, 2, GLES20.GL_FLOAT, false, 16, vertexBuffer.apply { position(2) })

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            GLES20.glViewport(0, 0, width, height)
        }

        private fun loadShader(type: Int, shaderCode: String): Int {
            return GLES20.glCreateShader(type).also { shader ->
                GLES20.glShaderSource(shader, shaderCode)
                GLES20.glCompileShader(shader)
            }
        }
    }

    @Composable
    fun RootView() {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
                .background(Color.Black)
        ) {
            AndroidView(
                factory = { context ->
                    return@AndroidView CustomizeGlSurface(context).also {
                        it.layoutParams =
                            ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT
                            )
                    }
                }
            )
        }
    }

    @Preview
    @Composable
    fun testPreview() {
        RootView()
    }
}