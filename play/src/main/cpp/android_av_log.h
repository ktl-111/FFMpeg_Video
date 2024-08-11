#ifndef VIDEOSDK_ANDROID_AV_LOG_H
#define VIDEOSDK_ANDROID_AV_LOG_H

#include <android/log.h>

extern "C" {
#include "libavutil/log.h"
}

#define ALOG(level, TAG, ...)    ((void)__android_log_vprint(level, TAG, __VA_ARGS__))
#define ALOGD(TAG, ...)    ALOG(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGV(TAG, ...)    ALOG(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGI(TAG, ...)    ALOG(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(TAG, ...)    ALOG(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(TAG, ...)    ALOG(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define LOG_TAG "video_sdk"

static void av_android_print_mapping(void *ptr, int level, const char *fmt, va_list vl) {
    switch (level) {
        case AV_LOG_DEBUG:
            ALOGD(LOG_TAG, fmt, vl);
            break;
        case AV_LOG_VERBOSE:
            ALOGV(LOG_TAG, fmt, vl);
            break;
        case AV_LOG_INFO:
            ALOGI(LOG_TAG, fmt, vl);
            break;
        case AV_LOG_WARNING:
            ALOGW(LOG_TAG, fmt, vl);
            break;
        case AV_LOG_ERROR:
            ALOGE(LOG_TAG, fmt, vl);
            break;
    }
}

static void av_android_log_init() {
    av_log_set_callback(av_android_print_mapping);
}

#endif //VIDEOSDK_ANDROID_AV_LOG_H
