package com.example.play.utils

import android.util.Log

/***********************************************************
 ** Copyright (C), 2008-2020, OPPO Mobile Comm Corp., Ltd.
 ** VENDOR_EDIT
 ** File:
 ** Description:
 ** Version:1.0
 ** Date : 2024.06.24
 ** Author:liubin
 **
 ** --------------------Revision History: ------------------
 ** <author>   <data>       <version >   <desc>
 ** liubin     2024.06.24   1.0
 ***********************************************************/
internal object LogHelper {
    private val logProxySets = mutableSetOf<LogProxy>()

    @Synchronized
    fun addLogProxy(logProxy: LogProxy) {
        logProxySets.add(logProxy)
    }

    @Synchronized
    fun removeLogProxy(logProxy: LogProxy) {
        logProxySets.remove(logProxy)
    }

    fun i(tag: String, msg: String) {
        println(Log.INFO, tag, msg)
    }

    fun d(tag: String, msg: String) {
        println(Log.DEBUG, tag, msg)
    }

    fun e(tag: String, msg: String) {
        println(Log.ERROR, tag, msg)
    }

    @Synchronized
    private fun println(priority: Int, tag: String, msg: String) {
        logProxySets.forEach {
            it.println(priority, tag, msg)
        }
    }
}

interface LogProxy {
    fun println(priority: Int, tag: String, msg: String)
}