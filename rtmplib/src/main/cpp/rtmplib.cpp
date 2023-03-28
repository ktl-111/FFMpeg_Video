#include <jni.h>
#include <string>
//ndk下log.h文件
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"RTMP_L",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"RTMP_L",__VA_ARGS__)

extern "C" {
//导包
#include "librtmp/rtmp.h"
}

typedef struct {
    int8_t *sps;
    int16_t spsLen;
    int8_t *pps;
    int16_t ppsLen;

    RTMP *rtmp;
} Live;

Live *live = nullptr;

int parseVideo(jbyte *data, jint len, jlong type);

int parseSpsPps(jbyte *data, jint len, Live *live);

int findFrame(jbyte *data, jint len);

void sendMetaDataPackage(Live *live);

int sendPacketByVideo(RTMPPacket *pPacket, int bodySize, jlong times);

int sendFramePackage(jbyte *data, jint len, jlong times);

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_rtmplib_RtmpLib_connectBiliService(JNIEnv *env, jobject thiz, jstring url_) {
    char *url = const_cast<char *>(env->GetStringUTFChars(url_, 0));
    int result;
    do {
        live = static_cast<Live *>(malloc(sizeof(Live)));
        memset(live, 0, sizeof(Live));
        //创建对象
        live->rtmp = RTMP_Alloc();
        //初始化
        RTMP_Init(live->rtmp);
        //设置超时时间10s
        live->rtmp->Link.timeout = 10;
        //设置url
        result = RTMP_SetupURL(live->rtmp, url);
        if (!result) {
            LOGE("RTMP_SetupURL filed");
            continue;
        }
        //开启写
        RTMP_EnableWrite(live->rtmp);
        //链接
        result = RTMP_Connect(live->rtmp, nullptr);
        if (!result) {
            LOGE("RTMP_Connect filed");
            continue;
        }
        //链接流
        result = RTMP_ConnectStream(live->rtmp, 0);
        if (!result) {
            LOGE("RTMP_ConnectStream filed");
            continue;
        }
        LOGE("connect success");
        break;
    } while (true);

    env->ReleaseStringUTFChars(url_, url);
    return result;
}

//解析视频
int parseVideo(jbyte *data, jint len, jlong times) {
    int result = 0;
    int offLength = 4;
    if (data[2] == 1) {
        offLength = 3;
    }
    data += offLength;
    len -= offLength;
    LOGI("data[0] %#x", data[0]);
    if (data[0] == 0x67) {
        LOGI("parse SPS and PPS");
        //判断live非null,并且live sps和pps没有值
        LOGI("live %#x,sps %s,pps %s", &live, live->sps, live->pps);
        if (live && ((!live->sps) || (!live->pps))) {
            result = parseSpsPps(data, len, live);
        }
    } else {
        if (data[0] == 0x65) {
            //I帧,需要发送rtmp sps+pps
            LOGI("parse I frame");
            sendMetaDataPackage(live);
        } else {
            LOGI("parse orther frame");
        }
        result = sendFramePackage(data, len, times);
    }
    return result;
}

int sendFramePackage(jbyte *data, jint len, jlong times) {
    auto *rtmpPacket = static_cast<RTMPPacket *>(malloc(sizeof(RTMPPacket)));
    int bodySize = 9 + len;
    //申请body内存大小
    RTMPPacket_Alloc(rtmpPacket, bodySize);
    int i = 0;
    if (data[0] == 0x65) {
        //I帧
        rtmpPacket->m_body[i++] = 0x17;
    } else {
        rtmpPacket->m_body[i++] = 0x27;
    }
    rtmpPacket->m_body[i++] = 0x01;
    rtmpPacket->m_body[i++] = 0x00;
    rtmpPacket->m_body[i++] = 0x00;
    rtmpPacket->m_body[i++] = 0x00;
    //4字节长度
    rtmpPacket->m_body[i++] = (len >> 24) & 0xFF;
    rtmpPacket->m_body[i++] = (len >> 16) & 0xFF;
    rtmpPacket->m_body[i++] = (len >> 8) & 0xFF;
    rtmpPacket->m_body[i++] = len & 0xFF;

    memcpy(&rtmpPacket->m_body[i], data, len);

    int result = sendPacketByVideo(rtmpPacket, bodySize, times);
    LOGI("send frame %s", result ? "success" : "fail");
    return result;
}

int sendPacketByVideo(RTMPPacket *pPacket, int bodySize, jlong times) {
    //设置参数
    pPacket->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pPacket->m_nBodySize = bodySize;
    //随意分配一个管道（尽量避开rtmp.c中使用的）
    pPacket->m_nChannel = 0x15;
    //不使用绝对时间
    pPacket->m_hasAbsTimestamp = 0;
    //sps设为0
    pPacket->m_nTimeStamp = times;

    pPacket->m_headerType = RTMP_PACKET_SIZE_LARGE;

    int result = RTMP_SendPacket(live->rtmp, pPacket, 1);
    //释放body
    RTMPPacket_Free(pPacket);
    //释放packet
    free(pPacket);
    return result;
}

//创建sps+pps rtmp包
void sendMetaDataPackage(Live *live) {
    auto *rtmpPacket = static_cast<RTMPPacket *>(malloc(sizeof(RTMPPacket)));
    int bodySize = 16 + live->spsLen + live->ppsLen;
    //申请body内存大小
    RTMPPacket_Alloc(rtmpPacket, bodySize);
    int i = 0;
    rtmpPacket->m_body[i++] = 0x17;
    rtmpPacket->m_body[i++] = 0x00;
    rtmpPacket->m_body[i++] = 0x00;
    rtmpPacket->m_body[i++] = 0x00;
    rtmpPacket->m_body[i++] = 0x00;

    rtmpPacket->m_body[i++] = 0x01;
    rtmpPacket->m_body[i++] = live->sps[1];//profile_id
    rtmpPacket->m_body[i++] = live->sps[2];//flag
    rtmpPacket->m_body[i++] = live->sps[3];//level

    //几个字节表示NALU长度,协议定义,(0xFF&3)+1=4,4字节表示长度
    rtmpPacket->m_body[i++] = 0xFF;
    //sps个数,0xE1&0x1F=1
    rtmpPacket->m_body[i++] = 0xE1;
    //SPS长度,两个字节
    rtmpPacket->m_body[i++] = (live->spsLen >> 8) & 0xFF;
    rtmpPacket->m_body[i++] = (live->spsLen) & 0xFF;

    memcpy(&rtmpPacket->m_body[i], live->sps, live->spsLen);
    //i偏移sps长度
    i += live->spsLen;
    //pps个数
    rtmpPacket->m_body[i++] = 0x01;
    //pps长度,两个字节
    rtmpPacket->m_body[i++] = (live->ppsLen >> 8) & 0xFF;
    rtmpPacket->m_body[i++] = (live->ppsLen) & 0xFF;

    memcpy(&rtmpPacket->m_body[i], live->pps, live->ppsLen);

    if (sendPacketByVideo(rtmpPacket, bodySize, 0)) {
        LOGI("send metadata success");
    } else {
        LOGI("send metadata fail");
    }
}

//解析sps和pps
int parseSpsPps(jbyte *data, jint len, Live *live) {
    int offIndex = findFrame(data, len);
    LOGI("parseSpsPps offIndex:%d", offIndex);
    if (!offIndex) {
        LOGE("parseSpsPps error,not find Separatist");
        return 0;
    }
    int offLen = 4;
    if (data[offIndex + 2] == 0x01) {
        offLen = 3;
    }
    //找到pps
    if (data[offIndex + offLen] == 0x68) {
        //pps的分隔符起点index=sps长度
        live->spsLen = offIndex;
        live->sps = static_cast<int8_t *>(malloc(live->spsLen));
        memcpy(live->sps, data, live->spsLen);


        live->ppsLen = len - offIndex - offLen;
        live->pps = static_cast<int8_t *>(malloc(live->ppsLen));
        //data移动sps长度+offlen
        memcpy(live->pps, data + live->spsLen + offLen, live->ppsLen);

        LOGI("parseSpsPps spsLen:%d ppsLen:%d data:%#x", live->spsLen, live->ppsLen, *data);
    }
    return 1;
}

int findFrame(jbyte *data, int len) {
    for (int i = 0; i < len; ++i) {
        if (i + 2 >= len) {
            return 0;
        }
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
            return i;
        }
        if (i + 3 >= len) {
            return 0;
        }
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            return i;
        }
    }
    return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_rtmplib_RtmpLib_sendData(JNIEnv *env, jobject thiz, jbyteArray data_, jint len_,
                                          jlong times_, jint type_) {
    int result = 0;
    jbyte *data = env->GetByteArrayElements(data_, 0);

    switch (type_) {
        case 0: {
            result = parseVideo(data, len_, times_);
            break;
        }
        case 1: {
            break;
        }
        default: {
            LOGE("not type %d", type_);
            break;
        }
    }

    env->ReleaseByteArrayElements(data_, data, 0);
    return result;
}