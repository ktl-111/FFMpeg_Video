package com.example.videolearn.ffmpegcompose.bean

import android.graphics.Bitmap
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf

data class VideoBean(
    val duration: Long, var bitmap: MutableState<Bitmap?> = mutableStateOf(null)
)
