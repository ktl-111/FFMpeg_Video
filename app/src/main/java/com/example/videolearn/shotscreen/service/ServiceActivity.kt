package com.example.videolearn.shotscreen.service

import android.os.Bundle
import android.view.View
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import com.example.videolearn.R

class ServiceActivity : AppCompatActivity() {
    lateinit var etProt: EditText
    var push: ProjectionH264Push? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ss_service)
        etProt = findViewById(R.id.et_prot)
    }

    fun start(v: View) {
        push = ProjectionH264Push(this, etProt.text.toString().trim())
            .apply {
                startPush()
            }
    }

    override fun onDestroy() {
        super.onDestroy()
        push?.release()
    }
}