package com.example.play.config

import androidx.annotation.Keep

/**
 * 如果有设置裁切宽高,onVideoConfig将会回调裁切宽高,否则回调输出宽高
 * @property width Int 输出width
 * @property height Int 输出height
 * @property cropWidth Int 基于输出width的裁切width
 * @property cropHeight Int 基于输出height的裁切height
 * @property fps Double
 * @constructor
 */
@Keep
data class OutConfig(
    val width: Int, val height: Int, val cropWidth: Int=0, val cropHeight: Int=0, val fps: Double
)