package com.example.videolearn

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.norman.android.hdrsample.util.LogUtils

open class BaseActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        LogUtils.i(this::class.java.simpleName, "onCreate")
    }

    override fun onResume() {
        super.onResume()
        LogUtils.i(this::class.java.simpleName, "onResume")
    }

    override fun onPause() {
        super.onPause()
        LogUtils.i(this::class.java.simpleName, "onPause")
    }

    override fun onDestroy() {
        super.onDestroy()
        LogUtils.i(this::class.java.simpleName, "onDestroy")
    }
}