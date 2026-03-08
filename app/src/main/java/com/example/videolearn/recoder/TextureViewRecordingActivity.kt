// TextureViewRecordingActivity.kt
package com.example.videolearn.recoder

import android.graphics.Color
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.TextureView
import android.widget.Button
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R
import java.util.concurrent.atomic.AtomicBoolean

class TextureViewRecordingActivity : AppCompatActivity(),
    TextureView.SurfaceTextureListener {

    private lateinit var textureView: TextureView
    private lateinit var contentLayout: LinearLayout
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button

    private var glRecorder: TextureViewRecorder? = null
    private val isRecording = AtomicBoolean(false)
    private val handler = Handler(Looper.getMainLooper())
    private var recordingRunnable: Runnable? = null
    private var isSurfaceAvailable = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_texture_view_recording)

        initViews()
        initSampleContent()
    }

    private fun initViews() {
        textureView = findViewById(R.id.texture_view)
        contentLayout = findViewById(R.id.content_layout)
        btnStart = findViewById(R.id.btn_start)
        btnStop = findViewById(R.id.btn_stop)

        textureView.surfaceTextureListener = this

        btnStart.setOnClickListener { startRecording() }
        btnStop.setOnClickListener { stopRecording() }
        btnStop.isEnabled = false
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d("Recording", "SurfaceTexture available: ${width}x${height}")
        isSurfaceAvailable = true

        // 初始绘制
        drawContentToTextureView()
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d("Recording", "SurfaceTexture size changed: ${width}x${height}")
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        Log.d("Recording", "SurfaceTexture destroyed")
        isSurfaceAvailable = false
        stopRecording()
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {
        // SurfaceTexture更新时调用
    }

    private fun drawContentToTextureView() {
        if (!isSurfaceAvailable) return

        val canvas = textureView.lockCanvas()
        try {
            // 清除画布
            canvas?.drawColor(Color.WHITE)

            // 绘制布局内容
            contentLayout.draw(canvas)
        } finally {
            canvas?.let {
                textureView.unlockCanvasAndPost(it)
            }
        }
    }

    private fun startRecording() {
        if (!isSurfaceAvailable || isRecording.getAndSet(true)) {
            return
        }

        Log.d("Recording", "Start recording")

        // 更新UI状态
        btnStart.isEnabled = false
        btnStop.isEnabled = true

        // 确保有最新的绘制
        drawContentToTextureView()

        // 创建并启动录制器
        try {
            glRecorder = TextureViewRecorder(
                textureView.surfaceTexture!!,
                textureView.width,
                textureView.height
            )
            glRecorder?.start()
            Log.d("Recording", "GL Recorder started")
        } catch (e: Exception) {
            Log.e("Recording", "Failed to start recorder", e)
            stopRecording()
            return
        }

        // 开始录制循环
        startRecordingLoop()
    }

    private fun startRecordingLoop() {
        recordingRunnable = object : Runnable {
            override fun run() {
                if (!isRecording.get() || !isSurfaceAvailable) {
                    return
                }

                try {
                    // 1. 将内容绘制到TextureView
                    drawContentToTextureView()

                    // 2. 通知录制器捕获帧（这里实际上不需要，因为录制器自己会循环）

                    // 3. 继续下一帧（约30fps）
                    handler.postDelayed(this, 33)

                } catch (e: Exception) {
                    Log.e("Recording", "Error in recording loop", e)
                }
            }
        }

        handler.post(recordingRunnable!!)
    }

    private fun stopRecording() {
        if (!isRecording.getAndSet(false)) {
            return
        }

        Log.d("Recording", "Stop recording")

        // 移除录制循环
        recordingRunnable?.let {
            handler.removeCallbacks(it)
        }
        recordingRunnable = null

        // 停止录制器
        glRecorder?.stop()
        glRecorder = null

        // 更新UI状态
        btnStart.isEnabled = true
        btnStop.isEnabled = false
    }

    private fun initSampleContent() {
        // 创建示例内容
        for (i in 1..5) {
            val textView = androidx.appcompat.widget.AppCompatTextView(this).apply {
                text = "这是第 $i 个TextView"
                textSize = 18f
                setTextColor(Color.BLACK)
                setPadding(16.dpToPx(), 8.dpToPx(), 16.dpToPx(), 8.dpToPx())
            }
            contentLayout.addView(textView)

            val button = Button(this).apply {
                text = "按钮 $i"
                setOnClickListener {
                    // 按钮点击事件
                    Log.d("Recording", "Button $i clicked")
                }
            }
            contentLayout.addView(button)
        }
    }

    private fun Int.dpToPx(): Int = (this * resources.displayMetrics.density).toInt()

    override fun onDestroy() {
        super.onDestroy()
        stopRecording()
    }
}