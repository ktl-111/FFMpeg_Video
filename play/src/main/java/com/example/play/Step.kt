package com.example.play

/***********************************************************
 ** Copyright (C), 2008-2020, OPPO Mobile Comm Corp., Ltd.
 ** VENDOR_EDIT
 ** File:
 ** Description:
 ** Version:1.0
 ** Date : 2024.06.20
 ** Author:liubin
 **
 ** --------------------Revision History: ------------------
 ** <author>   <data>       <version >   <desc>
 ** liubin     2024.06.20   1.0
 ***********************************************************/
sealed class Step {
    object UnknownStep : Step() {
        override fun toString() = "UnknownStep"
    }

    object PlayStep : Step() {
        override fun toString() = "PlayStep"
    }

    object PauseStep : Step() {
        override fun toString() = "PauseStep"
    }

}
