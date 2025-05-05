package com.norman.android.hdrsample.util

import android.util.Log

object LogUtils {
    @JvmStatic
    fun i(tag: String, msg: String) {
        Log.i(tag, "[${Thread.currentThread()}] ${msg}")
    }
    @JvmStatic
    fun e(tag: String, msg: String) {
        Log.e(tag, "[${Thread.currentThread()}] ${msg}")
    }
}