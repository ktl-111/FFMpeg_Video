#include "Logger.h"

int Logger::mLogLevel = ANDROID_LOG_DEFAULT;

bool Logger::isLogD() {
    return ANDROID_LOG_DEBUG >= mLogLevel;
}

bool Logger::isLogI() {
    return ANDROID_LOG_INFO >= mLogLevel;
}

bool Logger::isLogW() {
    return ANDROID_LOG_WARN >= mLogLevel;
}

bool Logger::isLogE() {
    return ANDROID_LOG_ERROR >= mLogLevel;
}