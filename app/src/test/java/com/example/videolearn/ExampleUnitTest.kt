package com.example.videolearn

import com.example.videolearn.utils.H264Codec
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import org.junit.Test

import org.junit.Assert.*
import java.io.File

/**
 * Example local unit test, which will execute on the development machine (host).
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
class ExampleUnitTest {
    @Test
    fun addition_isCorrect() = runBlocking {
        val property = File(System.getProperty("user.dir")).parentFile
        val filePath = File(property, "videofile/24px.264")
        H264Codec(filePath).startCodec()
        delay(5 * 60 * 1000)
    }
}