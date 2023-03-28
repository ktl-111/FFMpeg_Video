package com.example.videolearn.utils

import android.content.Context

object ScreenUtils {
    @JvmStatic
    fun getScreenWidth(context: Context) = context.resources.displayMetrics.widthPixels

    @JvmStatic
    fun getScreenHeight(context: Context) = context.resources.displayMetrics.heightPixels
}