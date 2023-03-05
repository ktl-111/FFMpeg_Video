package com.example.videolearn

import android.app.Application

class App : Application() {
    companion object {
        var application: Application? = null
    }

    override fun onCreate() {
        super.onCreate()
        application = this
    }
}