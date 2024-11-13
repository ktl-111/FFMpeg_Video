#include <jni.h>
#include "Logger.h"
#include "FFMpegPlayer.h"

void my_logoutput(void *ptr, int level, const char *fmt, va_list vl) {
    LOGE(fmt, vl)
}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jint result = -1;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return result;
    }
    LOGI("JNI_OnLoad");
//    av_log_set_level(AV_LOG_TRACE);
//    av_log_set_callback(my_logoutput);
    return JNI_VERSION_1_4;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeInit(JNIEnv *env, jobject thiz) {
    auto *player = new FFMpegPlayer();
    player->init(env, thiz);
    return reinterpret_cast<long>(player);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativePrepare(JNIEnv *env, jobject thiz,
                                                      jlong native_manager, jstring path,
                                                      jobject surface,
                                                      jobject out_config) {
    auto *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    const char *c_path = env->GetStringUTFChars(path, nullptr);
    std::string s_path = c_path;
    bool result = false;
    if (pPlayer != nullptr) {
        result = pPlayer->prepare(env, s_path, surface, out_config);
    }
    if (c_path != nullptr) {
        env->ReleaseStringUTFChars(path, c_path);
    }
    return result;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeStart(JNIEnv *env, jobject thiz,
                                                    jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->start();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeStop(JNIEnv *env, jobject thiz,
                                                   jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->stop();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeResume(JNIEnv *env, jobject thiz,
                                                     jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->resume();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativePause(JNIEnv *env, jobject thiz,
                                                    jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->pause();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeRelease(JNIEnv *env, jobject thiz,
                                                      jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    delete pPlayer;
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeSeekTo(JNIEnv *env, jobject thiz,
                                                     jlong native_manager,
                                                     jlong seekTime) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        return pPlayer->seekTo(seekTime);
    }
    return false;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeSurfaceReCreate(JNIEnv *env, jobject thiz,
                                                              jlong native_manager,
                                                              jobject surface) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->surfaceReCreate(env, surface);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeSurfaceDestroy(JNIEnv *env, jobject thiz,
                                                             jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        pPlayer->surfaceDestroy(env);
    }
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_play_proxy_FFMpegProxy_getCurrTimestamp(JNIEnv *env, jobject thiz,
                                                         jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        return pPlayer->getCurrTimestamp();
    }
    return 0;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_play_proxy_FFMpegProxy_getPlayerState(JNIEnv *env, jobject thiz,
                                                       jlong native_manager) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        return pPlayer->getPlayerState();
    }
    return 0;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_utils_FFMpegUtils_nativeSetNativeLogLevel(JNIEnv *env, jobject thiz,
                                                                jint logLevel) {
    Logger::setLogLevel(logLevel);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_proxy_FFMpegProxy_nativeCutting(JNIEnv *env, jobject thiz,
                                                      jlong native_manager, jstring src_path,
                                                      jstring dest_path, jlong start_time,
                                                      jlong end_time, jobject out_config,
                                                      jobject cb) {
    FFMpegPlayer *pPlayer = reinterpret_cast<FFMpegPlayer *>(native_manager);
    if (pPlayer != nullptr) {
        const char *c_srcPath = env->GetStringUTFChars(src_path, nullptr);
        const char *c_desPath = env->GetStringUTFChars(dest_path, nullptr);
        pPlayer->cutting(env, c_srcPath, c_desPath, start_time, end_time, out_config, cb);
        env->ReleaseStringUTFChars(src_path, c_srcPath);
        env->ReleaseStringUTFChars(dest_path, c_desPath);
    }
}