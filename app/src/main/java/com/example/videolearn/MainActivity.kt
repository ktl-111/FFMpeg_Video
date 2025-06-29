package com.example.videolearn

import android.Manifest
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.MediaStore
import android.util.Log
import androidx.activity.compose.setContent
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.videolearn.compose.ComposeActivity
import com.example.videolearn.ffmpegcompose.FFMpegActivity
import com.example.videolearn.glvideo.GLVideoActivity
import com.example.videolearn.live.LiveActivity
import com.example.videolearn.shotscreen.ShotScreenActivity
import com.example.videolearn.test.ParseDataActivity
import com.example.videolearn.test.TestActivity
import com.example.videolearn.utils.ResultUtils
import com.example.videolearn.videocall.VideoCallActivity
import com.example.videolearn.videoplay.VideoPlayActiivty
import com.norman.android.hdrsample.HDRPlayActivity
import java.io.File


class MainActivity : AppCompatActivity() {
    private val TAG = "MainActivity"
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            rootView()
        }
    }

    data class Items(val name: String, val click: () -> Unit)

    @Composable
    private fun rootView() {
        LazyColumn {
            val items = mutableListOf(
                Items("compose test") {
                    startActivity(android.content.Intent(this@MainActivity, ComposeActivity::class.java))
                },
                Items("opengl") {
                    startActivity(android.content.Intent(this@MainActivity, com.example.videolearn.opengltest.OpenglActivity::class.java))
                },
                Items("HDR") {
                    ffmpeg(mHdrPickLauncher)
                },
                Items("glvideo") {
                    ffmpeg(mGLVideoLauncher)
                },
                Items("ffmpeg") {
                    ffmpeg(mMediaPickLauncher)
                },
                Items("test") {
                    test()
                },
                Items("投屏") {
                    startService(android.content.Intent(this@MainActivity, com.example.videolearn.MediaService::class.java))
                    shotScreen()
                },
                Items("视频通话") {
                    startService(android.content.Intent(this@MainActivity, com.example.videolearn.MediaService::class.java))
                    video()
                },
                Items("读取流显示") {
                    parsedata()
                },
                Items("videoplay") {
                    videoplay()
                },
                Items("直播") {
                    startService(android.content.Intent(this@MainActivity, com.example.videolearn.MediaService::class.java))
                    live()
                },
            )


            itemsIndexed(items) { index: Int, item: Items ->
                button(item.name, item.click)
            }
        }
    }

    private fun ffmpeg(launch: ActivityResultLauncher<PickVisualMediaRequest>) {
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
                launch.launch(
                    PickVisualMediaRequest(
                        ActivityResultContracts.PickVisualMedia.SingleMimeType(
                            items[index]
                        )
                    )
                )
                dialog.dismiss()
            }
            alertBuilder.create().show()
        }
    }

    private val mHdrPickLauncher =
        registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
            uri?.let { uri ->
                val uriToFileApiQ = uri2Path(this, uri)
                val file = File(cacheDir, "test.mp4")
                if (file.exists()) {
                    file.delete()
                }
                val copyTo = File(uriToFileApiQ).copyTo(File(cacheDir, "test.mp4"))
                Log.i(TAG, "onActivityResult: ${uriToFileApiQ} ${uri.path} ${copyTo}")
                startActivity(Intent(this, HDRPlayActivity::class.java)
                    .also { it.putExtra("filepath", copyTo.absolutePath) })
            }
        }
    private val mMediaPickLauncher =
        registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
            uri?.let { uri ->
                val uriToFileApiQ = uri2Path(this, uri)
                Log.i(TAG, "onActivityResult: ${uriToFileApiQ} ${uri.path}")
                startActivity(Intent(this, FFMpegActivity::class.java)
                    .also { it.putExtra("filepath", uriToFileApiQ) })
            }
        }
    private val mGLVideoLauncher =
        registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
            uri?.let { uri ->
                val uriToFileApiQ = uri2Path(this, uri)
                val file = File(cacheDir, "test.mp4")
                if (file.exists()) {
                    file.delete()
                }
                val copyTo = File(uriToFileApiQ).copyTo(File(cacheDir, "test.mp4"))
                Log.i(TAG, "onActivityResult: ${uriToFileApiQ} ${uri.path} ${copyTo}")
                startActivity(Intent(this, GLVideoActivity::class.java)
                    .also { it.putExtra("filepath", copyTo.absolutePath) })
            }
        }

    private fun uri2Path(context: Context, uri: Uri?): String? {
        if (uri == null) {
            return null
        }
        if (ContentResolver.SCHEME_FILE == uri.scheme) {
            return uri.path
        } else if (ContentResolver.SCHEME_CONTENT == uri.scheme) {
            val authority = uri.authority!!
            if (authority.startsWith("com.android.externalstorage")) {
                return Environment.getExternalStorageDirectory().toString() + "/" + uri.path!!
                    .split(":".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()[1]
            } else {
                var queryUri = MediaStore.Files.getContentUri("external")
                var querySelection = "_id=?"
                var queryParams: Array<String>? = null
                Log.i(TAG, "Uri2Path: authority:${authority} uri:$uri")
                if (uri.toString().contains("com.android.providers.media.photopicker")) {
                    queryUri = uri
                    querySelection = ""
                } else if (authority == "media") {
                    queryParams = arrayOf(uri.toString().substring(uri.toString().lastIndexOf('/') + 1))
                } else if (authority.startsWith("com.android.providers")) {
                    queryParams = arrayOf(DocumentsContract.getDocumentId(uri).split(":".toRegex())
                        .dropLastWhile { it.isEmpty() }
                        .toTypedArray()[1])
                }

                val contentResolver = context.contentResolver
                val cursor = contentResolver.query(queryUri, arrayOf(MediaStore.Files.FileColumns.DATA), querySelection, queryParams, null)
                if (cursor != null) {
                    cursor.moveToFirst()
                    try {
                        val idx = cursor.getColumnIndex(MediaStore.Files.FileColumns.DATA)
                        return cursor.getString(idx)
                    } catch (e: Exception) {
                    } finally {
                        cursor.close()
                    }
                }
            }
        }
        return null
    }

    class CustomizePickVisualMedia : ActivityResultContracts.PickVisualMedia() {
        override fun createIntent(context: Context, input: PickVisualMediaRequest): Intent {
            return if (!isPhotoPickerAvailable()) {
                Intent(Intent.ACTION_GET_CONTENT).also {
                    val mediaType = input.mediaType
                    val array = if (mediaType is SingleMimeType) {
                        arrayOf(mediaType.mimeType)
                    } else {
                        arrayOf("image/gif", "video/mp4")
                    }
                    it.type = "*/*"
                    it.putExtra(Intent.EXTRA_MIME_TYPES, array)
                }
            } else {
                super.createIntent(context, input)
            }
        }
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
