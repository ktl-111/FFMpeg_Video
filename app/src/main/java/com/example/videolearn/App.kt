package com.example.videolearn

import android.app.Application
import android.os.Handler
import android.os.Looper
import com.norman.android.hdrsample.util.AppUtil

class App : Application() {
    companion object {
        val handler = Handler(Looper.getMainLooper())
        var application: Application? = null
        fun runOnUi(runnable: Runnable) {
            handler.post(runnable)
        }
    }

    override fun onCreate() {
        super.onCreate()
        AppUtil.init(this)
        application = this
    }
}