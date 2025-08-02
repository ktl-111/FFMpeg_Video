package com.example.videolearn.compose

import android.os.Bundle
import android.util.Log
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import com.example.videolearn.BaseActivity
import com.example.videolearn.R

class ComposeActivity : BaseActivity() {
    private val TAG = "ComposeTest"
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ZoomableBitmap()
        }
    }

    @Composable
    fun ZoomableBitmap(
        modifier: Modifier = Modifier
    ) {
        var scale by remember { mutableStateOf(1f) }
        var offsetX by remember { mutableStateOf(0f) }
        var offsetY by remember { mutableStateOf(0f) }
        var parentSize by remember { mutableStateOf(IntSize(0, 0)) }
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Box(
                modifier = modifier
                    .width(200.dp)
                    .aspectRatio(113f / 190f)
//                    .size(DpSize(200.dp, 300.dp))
                    .border(width = 1.dp, color = Color.Red)
                    .onGloballyPositioned { coordinates ->
                        parentSize = coordinates.size
                        Log.i(TAG, "ZoomableBitmap: parentSize:${parentSize}")
                    }
                    .clipToBounds()
            ) {
                var imageSize by remember { mutableStateOf(IntSize(0, 0)) }

                Image(
                    painter = painterResource(R.drawable.ic_test),
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier
                        .fillMaxWidth()
                        .aspectRatio(113f / 190f)
                        .graphicsLayer(
                            scaleX = scale,
                            scaleY = scale,
                            translationX = offsetX,
                            translationY = offsetY
                        )
                        .onGloballyPositioned { coordinates ->
                            imageSize = coordinates.size
                            Log.i(TAG, "ZoomableBitmap: imageSize:${imageSize}")
                        }
                        .pointerInput(Unit) {
                            detectTransformGestures(
                                onGesture = { centroid, pan, zoom, _ ->
                                    // 双指居中缩放
                                    val prevScale = scale
                                    val tempScale = scale * zoom
                                    var tempOffSetX = offsetX
                                    var tempOffSetY = offsetY
                                    if (zoom != 1f) {

                                        // 计算以中心点缩放时的偏移调整
                                        val compensationX = (centroid.x - imageSize.width / 2) * (tempScale - prevScale)
                                        val compensationY = (centroid.y - imageSize.height / 2) * (tempScale - prevScale)

                                        tempOffSetX = (tempOffSetX * zoom) + compensationX
                                        tempOffSetY = (tempOffSetY * zoom) + compensationY
                                    }
                                    // 单指拖拽
                                    else {
                                        tempOffSetX += pan.x
                                        tempOffSetY += pan.y
                                    }
                                    // 边界限制 - 确保图像边缘不能小于父布局边缘
                                    val scaleSize = Size((imageSize.width * tempScale), (imageSize.height * tempScale))

                                    // 计算允许的最大偏移量
                                    val maxX = ((scaleSize.width - parentSize.width) / 2).coerceAtLeast(0f)
                                    val maxY = ((scaleSize.height - parentSize.height) / 2).coerceAtLeast(0f)
                                    Log.i(TAG, "ZoomableBitmap:  maxX:$maxX maxY:$maxY scaleSize:${scaleSize} imageSize:${imageSize}")
                                    if (zoom != 1f && (maxX <= 0 || maxY <= 0)) {
                                        return@detectTransformGestures
                                    }
                                    if (maxX > 0) {
                                        offsetX = tempOffSetX.coerceIn(-maxX, maxX)
                                    }
                                    if (maxY > 0) {
//                                    // 限制偏移范围
                                        offsetY = tempOffSetY.coerceIn(-maxY, maxY)
                                    }
                                    scale = tempScale
                                    Log.i(TAG, "ZoomableBitmap centroid:${centroid} pan:$pan zoom:${zoom} offsetX:${offsetX} offsetY:${offsetY}  parentSize:${parentSize} imageSize:${imageSize} scale:${scale}")

                                }
                            )
                        }
                )
            }
        }
    }


}