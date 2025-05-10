package com.example.videolearn.opengl

import android.opengl.GLES20
import android.opengl.GLSurfaceView
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class CustomizeRender : GLSurfaceView.Renderer {
    private val vertexShaderCode =
        "attribute vec2 a_Position;  // 仅需XY坐标\n" +
                "attribute vec2 a_TexCoord;\n" +
                "varying vec2 v_TexCoord;\n" +
                "uniform mat4 u_MVPMatrix;   // 仍需要4x4矩阵\n" +
                "\n" +
                "void main() {\n" +
                "    v_TexCoord = a_TexCoord;\n" +
                "    gl_Position = u_MVPMatrix * vec4(a_Position, 0.0, 1.0); // Z固定为0\n" +
                "}"

    private val fragmentShaderCode =
        "precision mediump float;\n" +
                "uniform sampler2D u_Texture;\n" +
                "varying vec2 v_TexCoord;\n" +
                "\n" +
                "void main() {\n" +
                "    gl_FragColor = texture2D(u_Texture, v_TexCoord);\n" +
                "}"

    private val triangleCoords = floatArrayOf(
        -1f,  1f,
        -1f, -1f,
        1f, -1f,
        1f,  1f
    )

    private val color = floatArrayOf(1.0f, 0.0f, 0.0f, 1.0f) // RGBA红色

    private var program: Int = 0
    private var positionHandle: Int = 0
    private var colorHandle: Int = 0

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        // [1] 是什么：设置清屏颜色
        // [2] 为什么：避免帧残留
        // [3] 作用：指定glClear使用的颜色值
        // [4] 参数：RGBA范围0-1
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f)

        // 创建着色器程序
        val vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexShaderCode)
        val fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentShaderCode)

        // [1] 是什么：创建着色器程序
        // [2] 为什么：需要链接顶点和片元着色器
        // [3] 作用：生成最终可执行程序
        // [4] 相关API：glAttachShader/glLinkProgram
        program = GLES20.glCreateProgram().also {
            GLES20.glAttachShader(it, vertexShader)
            GLES20.glAttachShader(it, fragmentShader)
            GLES20.glLinkProgram(it)
        }

        // 获取属性位置
        positionHandle = GLES20.glGetAttribLocation(program, "vPosition")
        colorHandle = GLES20.glGetUniformLocation(program, "vColor")
    }

    override fun onDrawFrame(gl: GL10?) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)

        // [1] 是什么：使用着色器程序
        // [2] 为什么：后续操作基于该程序
        // [3] 作用：激活指定着色器
        // [4] 注意：切换程序需要重新调用
        GLES20.glUseProgram(program)

        // 准备顶点数据
        val vertexBuffer = ByteBuffer
            .allocateDirect(triangleCoords.size * 4)
            .order(ByteOrder.nativeOrder())
            .asFloatBuffer()
            .apply {
                put(triangleCoords)
                position(0)
            }

        // [1] 是什么：启用顶点属性
        // [2] 为什么：允许着色器访问顶点数据
        // [3] 作用：激活指定属性通道
        // [4] 相关：glDisableVertexAttribArray
        GLES20.glEnableVertexAttribArray(positionHandle)

        // [1] 是什么：顶点属性指针
        // [2] 为什么：指定数据格式
        // [3] 参数：索引/维度/类型/归一化/步长/偏移
        // [4] 注意：最后一个参数是ByteBuffer
        GLES20.glVertexAttribPointer(
            positionHandle,
            3,  // XYZ三维坐标
            GLES20.GL_FLOAT,
            false,
            12, // 每个顶点12字节（3float*4byte）
            vertexBuffer
        )

        // 设置颜色
        GLES20.glUniform4fv(colorHandle, 1, color, 0)

        // [1] 是什么：绘制三角形
        // [2] 为什么：提交绘制命令
        // [3] 参数：图元类型/起始索引/顶点数
        // [4] 其他模式：GL_LINE_LOOP等
        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, 3)

        GLES20.glDisableVertexAttribArray(positionHandle)
    }

    private fun loadShader(type: Int, shaderCode: String): Int {
        return GLES20.glCreateShader(type).also { shader ->
            GLES20.glShaderSource(shader, shaderCode)
            GLES20.glCompileShader(shader)
        }
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        // [1] 是什么：设置视口
        // [2] 为什么：适配屏幕尺寸变化
        // [3] 参数：左下角坐标/宽高
        // [4] 注意：通常与surface尺寸一致
        GLES20.glViewport(0, 0, width, height)
    }
}
