package com.example.videolearn.play

sealed class VideoModel(private val textureIndex: Int, private var option: Int) {

    companion object {
        private const val PLAY_OPTION = 1 shl 0
        private const val TRACK_OPTION = 1 shl 1
        private const val CUTTING_OPTION = 1 shl 2
    }

    operator fun plus(videoModel: VideoModel) {
        this.option = this.option and videoModel.option
    }

    object Play : VideoModel(0, PLAY_OPTION) {
        override fun toString() = "Play"
    }

    object Track : VideoModel(1, TRACK_OPTION) {
        override fun toString() = "Track"
    }

    object Cutting : VideoModel(2, CUTTING_OPTION) {
        override fun toString() = "Cutting"
    }
}