package com.example.play

/***********************************************************
 ** Copyright (C), 2008-2020, OPPO Mobile Comm Corp., Ltd.
 ** VENDOR_EDIT
 ** File:
 ** Description:
 * enum PlayerState {
 *     UNKNOWN,
 *     PREPARE,
 *     START,
 *     PLAYING,
 *     PAUSE,
 *     SEEK,
 *     STOP
 * };
 ** Version:1.0
 ** Date : 2024.01.02
 ** Author:liubin
 **
 ** --------------------Revision History: ------------------
 ** <author>   <data>       <version >   <desc>
 ** liubin     2024.01.02   1.0
 ***********************************************************/
sealed class PlayerState(val state: Int) {
    companion object {
        fun fromState(state: Int): PlayerState = when (state) {
            Prepare.state -> Prepare
            Playing.state -> Playing
            Pause.state -> Pause
            Stop.state -> Stop
            else -> Unknown
        }
    }

    object Unknown : PlayerState(0) {
        override fun toString() = "Unknown"
    }

    object Prepare : PlayerState(1) {
        override fun toString() = "Prepare"
    }

    object Playing : PlayerState(3) {
        override fun toString() = "Playing"
    }

    object Pause : PlayerState(4) {
        override fun toString() = "Pause"
    }

    object Stop : PlayerState(6) {
        override fun toString() = "Stop"
    }
}