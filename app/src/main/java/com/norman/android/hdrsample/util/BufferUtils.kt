package com.norman.android.hdrsample.util

import android.graphics.Bitmap
import androidx.core.graphics.createBitmap
import java.nio.ByteBuffer

fun ByteBuffer.toBitmap(width: Int, height: Int): Bitmap {
    val bitmap = createBitmap(width, height)
    rewind()
    bitmap.copyPixelsFromBuffer(this)
    return bitmap
}