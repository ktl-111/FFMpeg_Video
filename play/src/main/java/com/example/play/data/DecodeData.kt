package com.example.play.data

import androidx.annotation.Keep

/***********************************************************
 ** Copyright (C), 2008-2020, OPPO Mobile Comm Corp., Ltd.
 ** VENDOR_EDIT
 ** File:
 ** Description:解码数据
 ** Version:1.0
 ** Date : 2024.11.30
 ** Author:liubin
 **
 ** --------------------Revision History: ------------------
 ** <author>   <data>       <version >   <desc>
 ** liubin     2024.11.30   1.0
 ***********************************************************/
@Keep
data class DecodeData(var phoneData: PhoneData? = null, var useHwDecode: Boolean = false, var width: Int = 0, var height: Int = 0, var duration: Double = 0.0, var fps: Int = 0, var rotate: Int = 0, var prepareMsg: String = "",
                      var codeName: String = "", var frameDatas: ArrayList<DecodeFrameData> = arrayListOf())

@Keep
data class PhoneData(var model: String, var cpuv8: Boolean,var androidVersion:String, var processorsCount: Int = 0)

@Keep
data class DecodeFrameData(var decodeResult: String = "", var decodeCostTime: Double = 0.0,
                           var sendPacketCount: Int = 0, var frameFormat: String = "", var pictType: Int = 0)
