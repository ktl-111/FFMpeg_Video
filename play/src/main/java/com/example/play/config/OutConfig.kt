package com.example.play.config

import androidx.annotation.Keep

/**
 * 如果有设置裁切宽高,onVideoConfig将会回调裁切宽高,否则回调输出宽高
 * @property width Int 输出width,视频尺寸以该尺寸等比例缩放
 * @property height Int 输出height,视频尺寸以该尺寸等比例缩放
 * @property cropWidth Int 裁切width
 * @property cropHeight Int 裁切height
 * @property fps Double
 * @constructor
 */
@Keep
data class OutConfig(
    val width: Int,
    val height: Int,
    val cropWidth: Int = 0,
    val cropHeight: Int = 0,
    val fps: Double
)