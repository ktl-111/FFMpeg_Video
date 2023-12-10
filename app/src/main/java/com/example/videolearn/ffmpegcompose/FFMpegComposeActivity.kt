package com.example.videolearn.ffmpegcompose

import android.Manifest
import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.Build
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
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.constraintlayout.compose.ChainStyle
import androidx.constraintlayout.compose.ConstraintLayout
import androidx.constraintlayout.compose.Dimension
import androidx.core.graphics.drawable.toBitmap
import com.example.play.IPalyListener
import com.example.play.PlayManager
import com.example.play.utils.FFMpegUtils
import com.example.videolearn.MediaScope
import com.example.videolearn.R
import com.example.videolearn.ffmpegcompose.bean.VideoBean
import com.example.videolearn.utils.DisplayUtil
import com.example.videolearn.utils.ResultUtils
import com.luck.picture.lib.basic.PictureSelector
import com.luck.picture.lib.config.SelectMimeType
import com.luck.picture.lib.entity.LocalMedia
import com.luck.picture.lib.interfaces.OnResultCallbackListener
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File
import java.nio.ByteBuffer
import java.util.concurrent.Executors
import kotlin.math.abs


class FFMpegComposeActivity : AppCompatActivity() {
    val TAG = "FFMPEGActivity"
    private lateinit var surface: Surface
    private lateinit var surfaceView: SurfaceView
    private var playManager: PlayManager? = null
    private var path: String? = null
    private val videoList = mutableStateListOf<VideoBean>()
    private var mVideoDuration = 0L
    private val mFps = mutableStateOf(0)
    private val mCurrPlayTime = mutableStateOf(0f)
    private var isSeek = mutableStateOf(false)

    val itemSize = 40
    private val btnList = mutableStateListOf<BtnBean>().also {
        it.add(BtnBean("select") {
            select()
        })
        it.add(BtnBean("cutting") {
            cutting()
        })
//        it.add(BtnBean("testSeek") {
//            testSeek()
//        })
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
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { rootView(videoList, mFps, mCurrPlayTime) }
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
        path = File(Environment.getExternalStorageDirectory(), "douyon.mp4").absolutePath
//        path = File(Environment.getExternalStorageDirectory(), "VID_20230511_004231.mp4").absolutePath
//        path = File(application.externalCacheDir, "vid_test1.mp4").absolutePath
    }

    private fun playtest() {
        val list = mutableListOf<VideoBean>().apply {
            for (time in 0..30L) {
                add(VideoBean(time))
            }
        }
        videoList.addAll(list)
        MediaScope.launch(Dispatchers.Main) {
            delay(5000)
            val videoBean = videoList.get(5)
            videoBean.bitmap.value = getDrawable(R.drawable.ic_test)?.toBitmap()
            Log.i(TAG, "bitmap udpate :${videoBean}")
        }
    }

    private fun start() {
        val path = path!!
        if (!path.startsWith("http")) {
            if (!File(path).exists()) {
                Toast.makeText(this, "文件不存在", Toast.LENGTH_SHORT).show()
                return
            }
        }
        playManager ?: let {
            PlayManager().apply {
                playManager = this
                init(object : IPalyListener {
                    override fun onVideoConfig(
                        witdh: Int,
                        height: Int,
                        duration: Double,
                        fps: Double
                    ) {
                        val ratio = witdh.toFloat() / height
                        Log.i(
                            TAG, "onConfig: video width:$witdh,height$height ratio:$ratio\n" +
                                    "view width:${surfaceView.measuredWidth},height${surfaceView.measuredHeight}"
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
                        }
//                        initGetVideoFrames()
                    }

                    override fun onPalyProgress(time: Double) {
//                        updateUi(time.toFloat() / 1000)
                    }

                    override fun onPalyComplete() {
                    }

                })
                perpare(path, surface)
            }
        }.start()
    }

    private fun stop() {
        Log.i(TAG, "stop: ")
        playManager?.stop()
    }

    private fun resume() {
        Log.i(TAG, "resume: ")
        playManager?.resume()
    }

    private fun pause() {
        Log.i(TAG, "pause: ")
        playManager?.pause()
    }

    private fun updateUi(time: Float) {
        MediaScope.launch(Dispatchers.Main) {
            mCurrPlayTime.value = time
            Log.i(TAG, "updateUi: ${time}")
        }
    }

    private fun select() {
        val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Manifest.permission.READ_MEDIA_VIDEO
        } else {
            Manifest.permission.READ_EXTERNAL_STORAGE
        }
        ResultUtils.getInstance(this).singlePermissions(
            permission
        ) { result ->
            Log.i(TAG, "select: $result")
            if (!result) {
                return@singlePermissions
            }
            PictureSelector.create(this)
                .openSystemGallery(SelectMimeType.ofVideo())
                .forSystemResult(object : OnResultCallbackListener<LocalMedia> {
                    override fun onResult(result: ArrayList<LocalMedia>) {
                        path = result.get(0).availablePath
                        Log.i(TAG, "onResult: ${path}")
                    }

                    override fun onCancel() {

                    }
                })
        }
    }

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
            playManager?.apply {
                seekTo(seek.toDouble())
            }
        }
    }

    private fun cutting() {
        path?.also {
            if (it.isNotEmpty() && !it.startsWith("http")) {
                ResultUtils.getInstance(this)
                    .singlePermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE) { reuslt ->
                        Log.i(TAG, "cutting: $reuslt")
                        if (!reuslt) {
                            return@singlePermissions
                        }
//                        val outFile = File(Environment.getExternalStorageDirectory(), "testout.mp4")
                        val outFile = File(application.externalCacheDir, "testout.mp4")
                        if (!outFile.exists()) {
                            outFile.createNewFile()
                        } else {
                            outFile.delete()
                        }
                        Log.i(TAG, "cutting file:${outFile.absolutePath}")
                        val destPath = outFile.absolutePath
                        FFMpegUtils.cutting(path!!, destPath, 3.toDouble(), 10.toDouble(), 24)
                    }
            }
        }
    }

    val singleCoroutine = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
    var preTime = 0f

    private fun testSeek() {
        MediaScope.launch {
            var seek = 0f
            val diff = 1f / mFps.value
            while (seek < mVideoDuration) {
//                ffMpegPlay?.seekTo(seek)
                delay(50)
                seek += diff
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
                true,
                object :
                    FFMpegUtils.VideoFrameArrivedInterface {
                    override fun onStart(duration: Double): DoubleArray {
                        val size = duration.toInt()
                        Log.i(TAG, "onStart duration:${duration}")
                        val ptsArrays = DoubleArray(size)
                        for (i in 0 until size) {
                            ptsArrays[i] = i.toDouble()
                        }
                        val list = mutableListOf<VideoBean>().apply {
                            for (time in 0..duration.toLong()) {
                                add(VideoBean(time))
                            }
                        }
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
                        MediaScope.launch {
                            val videoBean = videoList[index]
                            Log.i(
                                TAG,
                                "onProgress pts:${pts} index:${index} rotate:${rotate} videoBean:${videoBean} ${width}*${height}"
                            )
                            val bitMap = frame.let {
                                val bitmap =
                                    Bitmap.createBitmap(
                                        width,
                                        height,
                                        Bitmap.Config.ARGB_8888
                                    )
                                bitmap!!.copyPixelsFromBuffer(it)
                                if (rotate != 0) {
                                    val matrix = Matrix()
                                    matrix.postRotate(rotate.toFloat())
                                    Bitmap.createBitmap(
                                        bitmap,
                                        0,
                                        0,
                                        bitmap.width,
                                        bitmap.height,
                                        matrix,
                                        true
                                    )
                                } else {
                                    bitmap
                                }
                            }
                            videoBean.also {
                                it.bitmap.value = bitMap
                            }
                        }
                        return false
                    }

                    override fun onEnd() {
                        Log.i(TAG, "onEnd: ")
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
//                                        pause()
                                    }
                                }
                            }
                        }
                    }) {
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
                                    start.linkTo(ivFrame.start)
                                    bottom.linkTo(ivFrame.top)
                                    end.linkTo(ivFrame.end)
                                },
                                text = "${item.duration}s"
                            )
                            val bitmap by remember {
                                item.bitmap
                            }
                            Image(
                                painter = bitmap?.let {
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
                                contentScale = ContentScale.Crop
                            )
                            Image(
                                painter = ColorPainter(Color.Red),
                                contentDescription = "",
                                modifier = Modifier
                                    .constrainAs(line) {
                                        start.linkTo(ivFrame.end)
                                        end.linkTo(parent.end)
                                        top.linkTo(tvTime.top)
                                        bottom.linkTo(ivFrame.bottom)
                                        height = Dimension.fillToConstraints
                                    }
                                    .width(1.dp)
                            )
                            createVerticalChain(tvTime, ivFrame, chainStyle = ChainStyle.Packed)
                        }
                    }
                }
            }

            LazyVerticalGrid(columns = GridCells.Fixed(4)) {
                items(btnList) {
                    commonButton(it.str, modifier = Modifier.weight(1f), it.onclick)
                }
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