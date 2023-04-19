#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"NATIVE_L",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"NATIVE_L",__VA_ARGS__)
