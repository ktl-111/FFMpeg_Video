package com.example.nativelib

object NativeLib {

    external fun connectBiliService(url:String): Boolean
    external fun sendData(data:ByteArray,len:Int,times:Long,type:Int): Boolean

    init {
        System.loadLibrary("nativelib")
    }
}