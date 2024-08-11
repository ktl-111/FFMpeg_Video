#ifndef LOGGER_H
#define LOGGER_H

#include <android/log.h>

class Logger {
private:
    static int mLogLevel;

public:
    static void setLogLevel(int logLevel) {
        mLogLevel = logLevel;
    }

    static bool isLogD();

    static bool isLogI();

    static bool isLogW();

    static bool isLogE();
};


#define TAG "FFMpeg-lib"

#define LOGD(...) if (Logger::isLogD()) { \
       __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__); \
    }
#define LOGI(...) if (Logger::isLogI()) { \
__android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__);\
}
#define LOGW(...) if (Logger::isLogW()) { \
__android_log_print(ANDROID_LOG_WARN,TAG,__VA_ARGS__); \
}
#define LOGE(...) if (Logger::isLogE()) { \
__android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__); \
}
#endif // LOGGER_H