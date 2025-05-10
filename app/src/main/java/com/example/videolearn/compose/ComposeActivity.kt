package com.example.videolearn.compose

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import com.example.videolearn.BaseActivity

class ComposeActivity : BaseActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
    }

    @Composable
    fun rootView() {
        Canvas(modifier = Modifier
            .width(100.dp)
            .height(30.dp)) {
            drawRect(color = Color.Red, style = Stroke(width = 1.dp.toPx()), size = Size(10.dp.toPx(), 10.dp.toPx()))
        }
    }
}