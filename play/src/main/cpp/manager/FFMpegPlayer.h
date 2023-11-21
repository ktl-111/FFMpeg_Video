//
// Created by Administrator on 2023/11/19.
//

#include <jni.h>
#include <string>
#include <sstream>
#include <ctime>
#include <thread>
#include "../utils/MutexObj.h"
#include "../utils/loghelper.h"
#include "../decoder/VideoDecoder.h"
#include "../decoder/AudioDecoder.h"
#include "../queue/AVPacketQueue.h"

extern "C" {
#include "../include/libavutil/avutil.h"
#include "../include/libavutil/frame.h"
#include "../include/libavutil/time.h"
#include "../include/libavformat/avformat.h"
#include "../include/libavcodec/avcodec.h"
#include "../include/libavcodec/jni.h"
}
#ifndef VIDEOLEARN_FFMPEGPLAY_H
#define VIDEOLEARN_FFMPEGPLAY_H

//定义context,调用java接口
typedef struct PlayerJniContext {
    jobject instance;
    jmethodID onVideoConfig;
    jmethodID onPlayProgress;
    jmethodID onPlayCompleted;

    void reset() {
        instance = nullptr;
        onVideoConfig = nullptr;
        onPlayProgress = nullptr;
        onPlayCompleted = nullptr;
    }

    bool isValid() {
        return instance != nullptr &&
               onPlayCompleted != nullptr &&
               onPlayProgress != nullptr
               && onVideoConfig != nullptr;
    }

} PlayerJniContext;

enum PlayerState {
    UNKNOWN,
    PREPARE,
    START,
    PLAYING,
    PAUSE,
    SEEK,
    STOP
};

class FFMpegPlayer {
public:
    FFMpegPlayer();

    ~FFMpegPlayer();

    void init(JNIEnv *env, jobject thiz);

    bool prepare(JNIEnv *env, std::string &path, jobject surface);

    void start();

    void resume();

    void pause();

    void stop();

    bool seekTo(double seekTime);

private:
    bool mHasAbort = false;
    bool mIsMute = false;
    bool mIsSeek = false;

    volatile double mVideoSeekPos = -1;
    volatile double mAudioSeekPos = -1;


    JavaVM *mJvm = nullptr;
    PlayerJniContext mPlayerJni{};
    std::shared_ptr<MutexObj> mMutexObj;

    volatile PlayerState mPlayerState = UNKNOWN;

    AVFormatContext *mAvFormatContext;

    std::thread *mReadPacketThread = nullptr;

    std::thread *mVideoThread = nullptr;
    std::shared_ptr<AVPacketQueue> mVideoPacketQueue = nullptr;
    std::shared_ptr<VideoDecoder> mVideoDecoder = nullptr;

    std::thread *mAudioThread = nullptr;
    std::shared_ptr<AVPacketQueue> mAudioPacketQueue = nullptr;
    std::shared_ptr<AudioDecoder> mAudioDecoder = nullptr;


    void VideoDecodeLoop();

    void AudioDecodeLoop();

    int readAvPacketToQueue();

    bool pushPacketToQueue(AVPacket *packet, const std::shared_ptr<AVPacketQueue> &queue) const;

    void ReadPacketLoop();

    void updatePlayerState(PlayerState state);

    void onPlayCompleted(JNIEnv *env);
};

#endif //VIDEOLEARN_FFMPEGPLAY_H
