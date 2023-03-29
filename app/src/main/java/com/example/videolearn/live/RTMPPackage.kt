package com.example.videolearn.live

const val RTMP_PACKET_TYPE_VIDEO = 0
const val RTMP_PACKET_TYPE_AUDIO = 1
const val RTMP_PACKET_TYPE_HEAD = 2

/**
 *
 * @property buffer
 * @property times 相对时间戳
 * @property type
 */
class RTMPPackage(val buffer: ByteArray, val times: Long, val type: Int)
