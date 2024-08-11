package com.example.videolearn

import android.Manifest
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.webkit.MimeTypeMap
import androidx.activity.compose.setContent
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.videolearn.ffmpegcompose.FFMpegActivity
import com.example.videolearn.live.LiveActivity
import com.example.videolearn.shotscreen.ShotScreenActivity
import com.example.videolearn.test.ParseDataActivity
import com.example.videolearn.test.TestActivity
import com.example.videolearn.utils.ResultUtils
import com.example.videolearn.videocall.VideoCallActivity
import com.example.videolearn.videoplay.VideoPlayActiivty
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class MainActivity : AppCompatActivity() {
    private val TAG = "MainActivity"
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
        ResultUtils.getInstance(this)
            .permissions(
                arrayOf(
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    android.Manifest.permission.READ_EXTERNAL_STORAGE
                ), object : ResultUtils.PermissionsCallBack {
                    override fun deniedList(deniedList: MutableList<String>?) {
                    }

                    override fun grantedList(grantedList: MutableList<String>?) {

                    }
                }
            )
        startService(Intent(this, MediaService::class.java))
    }

    @Composable
    private fun rootView() {
        Column {
            button("test") {
                test()
            }
            button("投屏") {
                shotScreen()
            }
            button("视频通话") {
                video()
            }
            button("读取流显示") {
                parsedata()
            }
            button("videoplay") {
                videoplay()
            }
            button("直播") {
                live()
            }
            button("ffmpeg") {
                ffmpeg()
            }
        }
    }

    private fun ffmpeg() {
        val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Manifest.permission.READ_MEDIA_VIDEO
        } else {
            Manifest.permission.READ_EXTERNAL_STORAGE
        }
        ResultUtils.getInstance(this).singlePermissions(permission) { result ->
            Log.i(TAG, "select: $result")
            if (!result) {
                return@singlePermissions
            }
            val items = arrayOf("video/mp4", "image/gif")
            val alertBuilder: AlertDialog.Builder = AlertDialog.Builder(this)
            alertBuilder.setTitle("select type")
            alertBuilder.setSingleChoiceItems(items, -1) { dialog, index ->
                mMediaPickLauncher.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.SingleMimeType(items[index])))
                dialog.dismiss()
            }
            alertBuilder.create().show()
        }
    }

    private val mMediaPickLauncher = registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        uri?.let { uri ->
            val uriToFileApiQ = uriToFileApiQ(uri, this)
            Log.i(TAG, "onActivityResult: ${uriToFileApiQ?.absolutePath} ${uri.path}")
            startActivity(Intent(this, FFMpegActivity::class.java).also { it.putExtra("filepath", uriToFileApiQ?.absolutePath) })
        }
    }

    fun uriToFileApiQ(uri: Uri?, context: Context): File? {
        var file: File? = null
        if (uri == null) return file
        //android10以上转换
        if (uri.scheme == ContentResolver.SCHEME_FILE) {
            file = File(uri.path)
        } else if (uri.scheme == ContentResolver.SCHEME_CONTENT) {
            //把文件复制到沙盒目录
            val contentResolver = context.contentResolver
            val displayName =
                (System.currentTimeMillis() + Math.round((Math.random() + 1) * 1000)).toString() + "." + MimeTypeMap.getSingleton()
                    .getExtensionFromMimeType(contentResolver.getType(uri))
            try {
                val `is` = contentResolver.openInputStream(uri)
                val cache = File(context.cacheDir.absolutePath, displayName)
                val fos = FileOutputStream(cache)
                `is`!!.copyTo(fos)
                file = cache
                fos.close()
                `is`.close()
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }
        return file
    }

    fun test() {
        startActivity(Intent(this, TestActivity::class.java))
    }

    fun shotScreen() {
        startActivity(Intent(this, ShotScreenActivity::class.java))
    }

    fun video() {
        startActivity(Intent(this, VideoCallActivity::class.java))
    }

    fun parsedata() {
        startActivity(Intent(this, ParseDataActivity::class.java))
    }

    fun videoplay() {
        startActivity(Intent(this, VideoPlayActiivty::class.java))
    }

    fun live() {
        //        startActivity(Intent(this, MainActivity::class.java))
        startActivity(Intent(this, LiveActivity::class.java))
    }

    @Composable
    private fun button(text: String, click: () -> Unit) = Button(
        onClick = click, modifier = Modifier
            .padding(10.dp)
            .fillMaxWidth()
    ) {
        Text(text = text)
    }

}
