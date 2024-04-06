package com.example.videolearn.ffmpegcompose

import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.Bundle
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
import androidx.compose.foundation.gestures.forEachGesture
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.material.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.constraintlayout.compose.ChainStyle
import androidx.constraintlayout.compose.ConstraintLayout
import androidx.constraintlayout.compose.Dimension
import com.example.play.IPalyListener
import com.example.play.PlayManager
import com.example.play.config.OutConfig
import com.example.play.utils.FFMpegUtils
import com.example.play.utils.MediaScope
import com.example.videolearn.ffmpegcompose.bean.VideoBean
import com.example.videolearn.utils.DisplayUtil
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import java.io.File
import java.nio.ByteBuffer
import java.util.concurrent.Executors


class FFMpegComposeActivity : AppCompatActivity() {
    val TAG = "FFMPEGActivity"
    private lateinit var surface: Surface
    private lateinit var surfaceView: SurfaceView
    private var playManager: PlayManager? = null
    private lateinit var path: String
    private val videoList = mutableStateListOf<VideoBean>()
    private val mFps = mutableStateOf(0)
    private val mVideoDuration = mutableStateOf(0.toDouble())
    private val mSize = mutableStateOf(Size(0f, 0f))
    private val mCurrPlayTime = mutableStateOf(0.toDouble())
    private val mCuttingProgress = mutableStateOf(0.toDouble())
    private var isSeek = mutableStateOf(false)

    val itemSize = 40
    private val btnList = mutableStateListOf<BtnBean>().also {
        it.add(BtnBean("cutting") {
            cutting()
        })
        it.add(BtnBean("start") {
            start()
        })
        it.add(BtnBean("stop") {
            stop()
        })
        it.add(BtnBean("resume") {
            resume()
        })
        it.add(BtnBean("pause") {
            pause()
        })
        it.add(BtnBean("currPlayer") {
            Toast.makeText(
                this@FFMpegComposeActivity,
                "player state:${playManager?.getPlayerState()}",
                Toast.LENGTH_SHORT
            ).show()
        })
        it.add(BtnBean("currTime") {
            Toast.makeText(
                this@FFMpegComposeActivity,
                "player state:${playManager?.getCurrTimestamp()}",
                Toast.LENGTH_SHORT
            ).show()
        })
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        //直播地址
//         path = "http://zhibo.hkstv.tv/livestream/mutfysrq/playlist.m3u8"
//         path = "http://39.135.138.58:18890/PLTV/88888888/224/3221225630/index.m3u8"
//        path = File(application.externalCacheDir, "testout_no_first_i.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "douyin.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "video.mp4").absolutePath
//        path = File(application.externalCacheDir, "ffmpeg3.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "ffmpeg.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "ffmpeg3.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "longvideo.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "douyon.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "gif_test_2.gif").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "gif_test_2.gif").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "test2.gif").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "VID_20230511_004231.mp4").absolutePath
//        path = File(application.externalCacheDir, "vid_test1.mp4").absolutePath
        path = intent.getStringExtra("filepath") ?: ""
//        path = File(application.externalCacheDir, "404.gif").absolutePath
        setContent {
            rootView(
                videoList, mFps, mCurrPlayTime, mSize, mVideoDuration, mCuttingProgress
            )
        }
    }

    private fun prepare(path: String?, surface: Surface) {
        if (path == null || !File(path).exists()) {
            Toast.makeText(this, "文件不存在", Toast.LENGTH_SHORT).show()
            return
        }
        playManager ?: let {
            PlayManager().apply {
                playManager = this
                init(object : IPalyListener {
                    override fun onVideoConfig(
                        witdh: Int, height: Int, duration: Double, fps: Double
                    ) {
                        val ratio = witdh.toFloat() / height
                        Log.i(
                            TAG,
                            "onConfig: video width:$witdh,height$height ratio:$ratio duration:${duration}\n view width:${surfaceView.measuredWidth},height${surfaceView.measuredHeight}"
                        )
                        val videoHeight: Int
                        val videoWidth: Int
                        if (height > witdh) {
                            videoHeight = surfaceView.measuredHeight
                            videoWidth = (videoHeight * ratio).toInt()
                        } else {
                            videoWidth = surfaceView.measuredWidth
                            videoHeight = (videoWidth / ratio).toInt()
                        }
                        MediaScope.launch(Dispatchers.Main) {
                            surfaceView.layoutParams.also {
                                it.width = videoWidth
                                it.height = videoHeight
                            }
                            surfaceView.requestLayout()

                            mFps.value = fps.toInt()
                            mSize.value = Size(witdh.toFloat(), height.toFloat())
                            mVideoDuration.value = duration
                        }
                        initGetVideoFrames()
                    }

                    override fun onPalyProgress(time: Double) {
                        updateUi(time / 1000)
                    }

                    override fun onPalyComplete() {
                        Log.i(TAG, "onPalyComplete: ${playManager?.getPlayerState()}")
                        MediaScope.launch(Dispatchers.Main) {
                            Toast.makeText(
                                this@FFMpegComposeActivity, "onPalyComplete", Toast.LENGTH_SHORT
                            ).show()
                        }
                    }

                })
                prepare(path, surface, outConfig)
//                prepare(path, surface)
            }
        }
    }

    private val outConfig = OutConfig(1280, 720, 378, 496, fps = 24.toDouble())

    private fun surfaceReCreate(surface: Surface) {
        playManager?.surfaceReCreate(surface)
    }

    private fun surfaceDestroy() {
        playManager?.surfaceDestroy()
    }

    private fun start() {
        Log.i(TAG, "start: ")
        playManager?.start()
    }

    private fun stop() {
        Log.i(TAG, "stop: ")
        playManager?.stop()
        playManager = null
        finish()
    }

    private fun resume() {
        Log.i(TAG, "resume: ")
        playManager?.resume()
    }

    private fun pause() {
        Log.i(TAG, "pause: ")
        playManager?.pause()
    }

    private fun updateUi(time: Double) {
        MediaScope.launch(Dispatchers.Main) {
            mCurrPlayTime.value = time
            Log.i(TAG, "updateUi: ${time}")
        }
    }

    val singleCoroutine = Executors.newSingleThreadExecutor().asCoroutineDispatcher()

    private fun seek(index: Int, offset: Int, itemWidthPx: Float) {
        if (!isSeek.value) {
            return
        }
        val scale = offset / itemWidthPx.toDouble()
        val seek = 1 * scale + index
        Log.i(TAG, "ui seek: ${seek}")
        updateUi(seek)
        playManager?.seekTo((seek * 1000).toLong())

//            if (seek == preTime) {
//                Log.i(TAG, "itemChange seek == preTime")
//                return@launch
//            }
//            mCurrPlayTime.value = seek
//            if (seek != 0.toDouble()) {
//                val diffSeek = abs(seek - preTime)
//                val minSeek = 1f / mFps.value
//                if (diffSeek < minSeek) {
//                    Log.i(TAG, "itemChange filter diffSeek:$diffSeek minSeek:${minSeek}")
//                    return@launch
//                }
//            }
//            preTime = seek
//
//            Log.i(TAG, "itemChange seek:$seek index:$index offset:$offset")
//            playManager?.apply {
//                seekTo(seek * 1000)
//            }
    }

    private fun uiSeekTo(seekTime: Double) {
        MediaScope.launch(singleCoroutine) {
            Log.i(TAG, "seekto: ${seekTime}")
            playManager?.apply {
                mCurrPlayTime.value = seekTime
                seekTo((seekTime * 1000).toLong())
//                mCurrPlayTime.value -= 1.0f / mFps.value
//                seekTo((mCurrPlayTime.value * 1000).toLong())
            }
        }
    }

    private fun cutting() {
        path?.also {
            MediaScope.launch {
                if (it.isNotEmpty() && !it.startsWith("http")) {
//                        val outFile = File(Environment.getExternalStorageDirectory(), "testout.mp4")
                    val outFile = File(application.externalCacheDir, "testout.mp4")
                    if (!outFile.exists()) {
                        outFile.createNewFile()
                    } else {
                        outFile.delete()
                    }
                    Log.i(TAG, "cutting file:${outFile.absolutePath}")
                    val destPath = outFile.absolutePath
                    val startTime = 0 * 1000
                    val allTime = 5.0 * 1000
                    FFMpegUtils.cutting(path,
                        destPath,
                        startTime.toLong(),
                        (startTime + allTime).toLong(),
                        outConfig,
                        object : FFMpegUtils.VideoCuttingInterface {
                            override fun onStart() {
                                Log.i(TAG, "onStart: ")
                            }

                            override fun onProgress(progress: Double) {
                                Log.i(TAG, "onProgress: $progress")
                                MediaScope.launch(Dispatchers.Main) {
                                    mCuttingProgress.value = progress;
                                }
                            }

                            override fun onFail(resultCode: Int) {
                                Log.i(TAG, "onFail: $resultCode")
                            }

                            override fun onDone() {
                                Log.i(TAG, "onDone: ")
                            }

                        })
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        playManager?.also {
            it.stop()
            it.release()
        }
    }

    private fun initGetVideoFrames() {
        MediaScope.launch(Dispatchers.IO) {
            FFMpegUtils.getVideoFrames(path,
                DisplayUtil.dp2px(this@FFMpegComposeActivity, itemSize.toFloat()),
                0,
                false,
                object : FFMpegUtils.VideoFrameArrivedInterface {
                    override fun onStart(duration: Double): DoubleArray {
                        val size = Math.ceil(duration).toInt()
                        Log.i(TAG, "onStart duration:${duration} size:$size")
                        val ptsArrays = DoubleArray(size)
                        for (i in 0 until size) {
                            ptsArrays[i] = i.toDouble()
                        }
                        val list = mutableListOf<VideoBean>().apply {
                            for (time in 1..size.toLong()) {
                                add(VideoBean(time))
                            }
                        }
                        for (i in 0..15) {
                            list.add(VideoBean(-1))
                        }
                        videoList.clear()
                        videoList.addAll(list)
                        return ptsArrays
                    }

                    override fun onProgress(
                        frame: ByteBuffer,
                        pts: Double,
                        width: Int,
                        height: Int,
                        rotate: Int,
                        index: Int
                    ): Boolean {
                        Log.i(TAG, "onProgress pts:${pts}")
                        MediaScope.launch {
                            val videoBean = videoList[index]
                            Log.i(
                                TAG,
                                "onProgress pts:${pts} index:${index} rotate:${rotate} videoBean:${videoBean} ${width}*${height}"
                            )
                            val bitMap = frame.let {
                                val bitmap = Bitmap.createBitmap(
                                    width, height, Bitmap.Config.ARGB_8888
                                )
                                bitmap!!.copyPixelsFromBuffer(it)
                                if (rotate != 0) {
                                    val matrix = Matrix()
                                    matrix.postRotate(rotate.toFloat())
                                    Bitmap.createBitmap(
                                        bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true
                                    )
                                } else {
                                    bitmap
                                }
                            }
                            videoBean.also {
                                it.bitmap.value = bitMap
                            }
                        }
                        return playManager == null
                    }

                    override fun onEnd() {
                        Log.i(TAG, "getVideoFrames onEnd: ")
                    }
                })
        }
    }

    @Preview
    @Composable
    fun testPreview() {
        rootView(videoList.apply {
            add(VideoBean(1))
            add(VideoBean(2))
        }, mFps, mCurrPlayTime, mSize, mVideoDuration, mCuttingProgress)
    }

    @Composable
    fun rootView(
        videoList: SnapshotStateList<VideoBean>,
        fps: MutableState<Int>,
        currPlayTime: MutableState<Double>,
        size: MutableState<Size>,
        videoDuration: MutableState<Double>,
        cuttingProgress: MutableState<Double>,
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
                                    Log.i(TAG, "surfaceCreated: ")
                                    surface = holder.surface
                                    MediaScope.launch {
                                        if (playManager == null) {
                                            prepare(path, surface)
                                        } else {
                                            surfaceReCreate(surface)
                                        }
                                    }
                                }

                                override fun surfaceChanged(
                                    holder: SurfaceHolder, format: Int, width: Int, height: Int
                                ) {
                                }

                                override fun surfaceDestroyed(holder: SurfaceHolder) {
                                    Log.i(TAG, "surfaceDestroyed: ")
                                    surfaceDestroy()
                                }

                            })
                            it.layoutParams =
                                LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT)
                        }
                    },
                    modifier = Modifier
                        .border(width = 1.dp, color = Color.Red)
                        .align(Alignment.Center)
                )
                Text(
                    text = "fps:${fps.value},size:${size.value.width.toInt()}*${size.value.height.toInt()},duration:${
                        String.format(
                            "%.2f", videoDuration.value
                        )
                    },currTime:${
                        String.format(
                            "%.2f", currPlayTime.value
                        )
                    }", color = Color.Red
                )
            }
            Column(
                modifier = Modifier
                    .padding(10.dp)
                    .fillMaxWidth()
                    .wrapContentHeight()
            ) {
                val lazyListState = rememberLazyListState()
                val itemSizeDP = itemSize.dp
                val isSeek = remember {
                    isSeek
                }
                val itemWidthPx = LocalDensity.current.run {
                    itemSizeDP.toPx()
                }
                if (isSeek.value) {
                    val index = remember { derivedStateOf { lazyListState.firstVisibleItemIndex } }
                    val offset =
                        remember { derivedStateOf { lazyListState.firstVisibleItemScrollOffset } }
                    seek(index.value, offset.value, itemWidthPx)
                }
                if (lazyListState.isScrollInProgress) {
                    DisposableEffect(key1 = Unit) {
                        Log.i(TAG, "DisposableEffect: onStart")

                        onDispose {
                            isSeek.value = false
                            Log.i(TAG, "DisposableEffect: onDispose")
                        }
                    }

                }
                val time by remember {
                    currPlayTime
                }
                LaunchedEffect(key1 = time) {
                    if (lazyListState.isScrollInProgress || isSeek.value) {
                        Log.i(TAG, "LaunchedEffect isScrollInProgress or isSeek")
                        return@LaunchedEffect
                    }
                    val scrollIndex = (currPlayTime.value / 1).toInt()
                    val scrollOffset = (currPlayTime.value % 1 * itemWidthPx).toInt()
                    Log.i(
                        TAG,
                        "LaunchedEffect: scrollIndex:$scrollIndex scrollOffset:$scrollOffset"
                    )
                    lazyListState.scrollToItem(scrollIndex, scrollOffset)
                }
                LazyRow(state = lazyListState,
                    modifier = Modifier
                        .fillMaxWidth()
                        .pointerInput(Unit) {
                            forEachGesture {
                                awaitPointerEventScope {
                                    val event = awaitPointerEvent()
                                    Log.i(
                                        TAG,
                                        "awaitPointerEventScope event type:${event.type} ${event.changes.size}"
                                    )
                                    if (event.changes.size == 1) {
                                        if (event.type == PointerEventType.Press) {
                                            isSeek.value = true
//                                            pause()
                                        }
                                    }
                                }
                            }
                        }
                ) {
                    items(videoList) { item ->
                        ConstraintLayout(
                            modifier = Modifier
                                .wrapContentHeight()
                                .wrapContentWidth()
                        ) {
                            val (tvTime, ivFrame, line) = createRefs()

                            Text(
                                modifier = Modifier.constrainAs(tvTime) {
                                    top.linkTo(parent.top)
                                    bottom.linkTo(ivFrame.top)
                                    end.linkTo(ivFrame.end)
                                }, text = "${item.duration}s"
                            )
                            val bitmap by remember {
                                item.bitmap
                            }
                            Image(painter = bitmap?.let {
                                BitmapPainter(it.asImageBitmap())
                            } ?: kotlin.run {
                                ColorPainter(Color.Red)
                            },
                                contentDescription = "",
                                modifier = Modifier
                                    .constrainAs(ivFrame) {
                                        top.linkTo(tvTime.bottom)
                                        start.linkTo(parent.start)
                                        end.linkTo(parent.end)
                                        bottom.linkTo(parent.bottom)
                                    }
                                    .width(itemSizeDP)
                                    .height(itemSizeDP),
                                contentScale = ContentScale.Crop)
                            Image(painter = ColorPainter(Color.Red),
                                contentDescription = "",
                                modifier = Modifier
                                    .constrainAs(line) {
                                        start.linkTo(ivFrame.end)
                                        end.linkTo(parent.end)
                                        top.linkTo(tvTime.top)
                                        bottom.linkTo(ivFrame.bottom)
                                        height = Dimension.fillToConstraints
                                    }
                                    .width(1.dp))
                            createVerticalChain(tvTime, ivFrame, chainStyle = ChainStyle.Packed)
                        }
                    }
                }
            }

            LazyVerticalGrid(columns = GridCells.Fixed(3)) {
                items(btnList) {
                    val str = if (it.str.contains("cutting")) {
                        "${it.str}-${String.format("%.2f", cuttingProgress.value)}"
                    } else {
                        it.str
                    }
                    commonButton(str, modifier = Modifier.weight(1f), it.onclick)
                }
            }
            Row {
                var text by remember {
                    mutableStateOf("")
                }
                commonButton(text = "seek to", modifier = Modifier.weight(1.0f)) {
                    uiSeekTo(text.let {
                        if (it.isEmpty()) {
                            0.toDouble()
                        } else {
                            it.toDouble()
                        }
                    })
                }
                TextField(
                    value = text,
                    onValueChange = { text = it },
                    modifier = Modifier.weight(1.0f),
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
                )
            }
        }
    }

    data class BtnBean(val str: String, val onclick: () -> Unit)


    @Composable
    fun commonButton(text: String, modifier: Modifier, onclick: () -> Unit) {
        Button(
            modifier = Modifier
                .padding(0.dp, 10.dp, 0.dp, 0.dp)
                .fillMaxWidth()
                .then(modifier),
            onClick = onclick
        ) {
            Text(text = text)
        }
    }
}