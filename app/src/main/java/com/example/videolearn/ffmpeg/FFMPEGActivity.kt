package com.example.videolearn.ffmpeg

import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup.LayoutParams
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.nativelib.FFMpegPlay
import com.example.videolearn.MediaScope
import com.example.videolearn.ffmpeg.bean.VideoBean
import com.example.videolearn.utils.FileUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File
import java.util.concurrent.Executors
import kotlin.math.abs

class FFMPEGActivity : AppCompatActivity() {
    val TAG = "FFMPEGActivity"
    private lateinit var surface: Surface
    private lateinit var surfaceView: SurfaceView
    private var ffMpegPlay: FFMpegPlay? = null
    private var path: String? = null
    private val videoList = mutableStateListOf<VideoBean>()
    private var mVideoDuration = 0L
    private val mFps = mutableStateOf(0)
    private val mCurrPlayTime = mutableStateOf(0f)
    private var isSeek = mutableStateOf(false)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { rootView(videoList, mFps, mCurrPlayTime) }
    }

    private fun play() {
        //直播地址
//         path = "http://zhibo.hkstv.tv/livestream/mutfysrq/playlist.m3u8"
//         path = "http://39.135.138.58:18890/PLTV/88888888/224/3221225630/index.m3u8"
//        path = File(application.externalCacheDir, "testout_no_first_i.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "douyin.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "video.mp4").absolutePath
//        path = File(application.externalCacheDir, "ffmpeg3.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "ffmpeg.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "ffmpeg3.mp4").absolutePath
        path = File(Environment.getExternalStorageDirectory(), "longvideo.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "VID_20230511_004231.mp4").absolutePath
//        path = File(application.externalCacheDir, "vid_test1.mp4").absolutePath
        val path = path!!
        if (!path.startsWith("http")) {
            if (!File(path).exists()) {
                Toast.makeText(this, "文件不存在", Toast.LENGTH_SHORT).show()
                return
            }
        }
        val newSingleThreadExecutor = Executors.newSingleThreadExecutor()
        ffMpegPlay ?: let {
            FFMpegPlay(surfaceView).apply {
                ffMpegPlay = this
                val fileName = "ffmpeg"
                FileUtils.deleteBytesFile(fileName)
                FileUtils.deleteContentFile(fileName)
//                dataCallback = {
//                    newSingleThreadExecutor.submit {
//                        kotlin.runCatching {
//                            FileUtils.writeBytes(it, fileName)
//                            FileUtils.writeContent(it, fileName)
//                        }.onFailure {
//                            it.printStackTrace()
//                        }
//                    }
//                }
                configCallback = { duration, fps ->
                    mVideoDuration = duration
                    MediaScope.launch(Dispatchers.Main) {
                        mFps.value = fps
                        val list = mutableListOf<VideoBean>().apply {
                            for (time in 0..duration) {
                                add(VideoBean(time))
                            }
                        }
                        videoList.addAll(list)
                    }
                }
                playTimeCallback = { time ->
                    updateUi(time)
                }
            }
        }.play(path, surface = surface)
    }

    private fun updateUi(time: Float) {
        MediaScope.launch(Dispatchers.Main) {
            mCurrPlayTime.value = time
            Log.i(TAG, "updateUi: ${time}")
        }
    }

    private fun resume() {
        Log.i(TAG, "resume: ")
        MediaScope.launch {
            ffMpegPlay?.resume()
        }
    }

    private fun pause() {
        Log.i(TAG, "pause: ")
        ffMpegPlay?.pause()
    }

    private fun cutting() {
        path?.also {
            if (it.isNotEmpty() && !it.startsWith("http")) {
                MediaScope.launch {
                    val outFile = File(application.externalCacheDir, "testout.mp4")
                    if (!outFile.exists()) {
                        outFile.createNewFile()
                    } else {
                        outFile.delete()
                    }
                    Log.i(TAG, "cutting file:${outFile.absolutePath}")
                    val destPath = outFile.absolutePath
                    ffMpegPlay?.cutting(destPath)
                }
            }
        }
    }

    val singleCoroutine = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
    var preTime = 0f

    private fun seek(index: Int, offset: Int, itemWidthPx: Float) {
        if (!isSeek.value) {
            return
        }
        MediaScope.launch(singleCoroutine) {
            val scale = offset / itemWidthPx
            val seek = 1f * scale + index
            if (seek == preTime) {
                Log.i(TAG, "itemChange seek == preTime")
                return@launch
            }
            mCurrPlayTime.value = seek
            if (seek != 0f) {
                val diffSeek = abs(seek - preTime)
                val minSeek = 1f / mFps.value
                if (diffSeek < minSeek) {
                    Log.i(TAG, "itemChange filter diffSeek:$diffSeek minSeek:${minSeek}")
                    return@launch
                }
            }
            preTime = seek
            Log.i(TAG, "itemChange seek:$seek index:$index offset:$offset")
            ffMpegPlay?.seekTo(seek)
        }
    }

    private fun testSeek() {
        MediaScope.launch {
            var seek = 0f
            val diff = 1f / mFps.value
            while (seek < mVideoDuration) {
                ffMpegPlay?.seekTo(seek)
                delay(50)
                seek += diff
            }
        }

    }

    override fun onDestroy() {
        super.onDestroy()
        ffMpegPlay?.release()
    }

    @Preview
    @Composable
    fun testPreview() {
        rootView(videoList.apply {
            add(VideoBean(1))
            add(VideoBean(2))
        }, mFps, mCurrPlayTime)
    }

    @Composable
    fun rootView(
        videoList: SnapshotStateList<VideoBean>,
        fps: MutableState<Int>,
        currPlayTime: MutableState<Float>
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color.Black)
            ) {
                AndroidView(
                    factory = { context ->
                        return@AndroidView SurfaceView(context).also {
                            surfaceView = it
                            it.holder.addCallback(object : SurfaceHolder.Callback {
                                override fun surfaceCreated(holder: SurfaceHolder) {
                                    surface = holder.surface
                                }

                                override fun surfaceChanged(
                                    holder: SurfaceHolder,
                                    format: Int,
                                    width: Int,
                                    height: Int
                                ) {
                                }

                                override fun surfaceDestroyed(holder: SurfaceHolder) {
                                }

                            })
                            it.layoutParams =
                                LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT)
                        }
                    }, modifier = Modifier
                        .border(width = 1.dp, color = Color.Red)
                        .align(Alignment.Center)
                )
                Text(text = "fps:${fps.value}\ncurrTime:${currPlayTime.value}", color = Color.Red)
            }
            Column(
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
                    .wrapContentHeight()
            ) {
                val itemWidth = 80.dp
                val lazyListState = rememberLazyListState()
                val isSeek = remember {
                    isSeek
                }
                val itemWidthPx = LocalDensity.current.run {
                    itemWidth.toPx()
                }
                if (isSeek.value) {
                    val index = remember { derivedStateOf { lazyListState.firstVisibleItemIndex } }
                    val offset =
                        remember { derivedStateOf { lazyListState.firstVisibleItemScrollOffset } }
                    seek(index.value, offset.value, itemWidthPx)
                } else {
                    LaunchedEffect(key1 = derivedStateOf { currPlayTime }) {
                        val scrollIndex = (currPlayTime.value / 1).toInt()
                        val scrollOffset = (currPlayTime.value % 1 * itemWidthPx).toInt()
                        Log.i(
                            TAG,
                            "LaunchedEffect: scrollIndex:$scrollIndex scrollOffset:$scrollOffset"
                        )
                        lazyListState.scrollToItem(scrollIndex, scrollOffset)
                    }
                }
                LazyRow(state = lazyListState, modifier = Modifier
                    .fillMaxWidth()
                    .pointerInput(Unit) {
                        awaitPointerEventScope {
                            while (true) {
                                val event = awaitPointerEvent()
                                Log.i(TAG, "awaitPointerEventScope event type:${event.type}")
                                if (event.changes.size == 1) {
                                    if (event.type == PointerEventType.Release) {
                                        isSeek.value = false
                                        resume()
                                    } else if (event.type == PointerEventType.Press) {
                                        isSeek.value = true
                                        pause()
                                    }
                                }
                            }
                        }
                    }) {
                    items(videoList) {
                        Column(
                            modifier = Modifier
                                .wrapContentHeight()
                                .wrapContentWidth()
                        ) {
                            Box {
                                Box(
                                    modifier = Modifier
                                        .width(itemWidth)
                                        .height(30.dp),
                                    contentAlignment = Alignment.Center
                                ) {
                                    Text(
                                        text = "${it.duration}s"
                                    )
                                }
                                Image(
                                    painter = ColorPainter(Color.Black),
                                    contentDescription = "",
                                    modifier = Modifier
                                        .width(1.dp)
                                        .height(30.dp)
                                )
                            }

                        }

                    }
                }

                commonButton("cutting") {
                    cutting()
                }
                commonButton(text = "testSeek") {
                    testSeek()
                }
                commonButton("pause") {
                    pause()
                }
                commonButton("play") {
                    play()
                }
            }
        }
    }


    @Composable
    fun commonButton(text: String, onclick: () -> Unit) {
        Button(
            modifier = Modifier
                .padding(0.dp, 10.dp, 0.dp, 0.dp)
                .fillMaxWidth(),
            onClick = onclick
        ) {
            Text(text = text)
        }
    }

}