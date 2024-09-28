//
// Created by Administrator on 2023/11/19.
//

#include <jni.h>
#include <string>
#include <sstream>
#include <ctime>
#include <thread>
#include "../utils/MutexObj.h"
#include "Logger.h"
#include "../decoder/VideoDecoder.h"
#include "../decoder/AudioDecoder.h"
#include "../queue/AVPacketQueue.h"
#include "../queue/AVFrameQueue.h"

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
    jmethodID onPlayError;

    void reset() {
        instance = nullptr;
        onVideoConfig = nullptr;
        onPlayProgress = nullptr;
        onPlayCompleted = nullptr;
        onPlayError = nullptr;
    }

    bool isValid() {
        return instance != nullptr &&
               onPlayCompleted != nullptr &&
               onPlayProgress != nullptr
               && onVideoConfig != nullptr
               && onPlayError != nullptr;
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
enum ReadPackType {
    ANY,
    VIDEO,
    AUDIO
};

class FFMpegPlayer {
public:
    FFMpegPlayer();

    ~FFMpegPlayer();

    void init(JNIEnv *env, jobject thiz);

    bool prepare(JNIEnv *env, std::string &path, jobject surface, jobject out_config);

    void start();

    void resume();

    void pause();

    void stop();

    bool seekTo(int64_t seekTime);

    void surfaceReCreate(JNIEnv *env, jobject surface);

    void surfaceDestroy(JNIEnv *env);

    int getPlayerState();

    int64_t getCurrTimestamp();

private:
    bool mHasAbort = false;
    bool mIsMute = false;
    bool mIsSeek = false;
    bool mIsBackSeek = false;
    int64_t mCurrSeekTime = -1;
    bool mShowFirstFrame = true;

    JavaVM *mJvm = nullptr;
    PlayerJniContext mPlayerJni{};
    std::shared_ptr<MutexObj> mMutexObj = nullptr;

    volatile PlayerState mPlayerState = UNKNOWN;

    AVFormatContext *mAvFormatContext = nullptr;

    std::thread *mReadPacketThread = nullptr;
    std::thread *mVideoDecodeThread = nullptr;
    std::thread *mVideoThread = nullptr;

    std::shared_ptr<AVPacketQueue> mVideoPacketQueue = nullptr;
    std::shared_ptr<AVFrameQueue> mVideoFrameQueue = nullptr;
    std::shared_ptr<VideoDecoder> mVideoDecoder = nullptr;

    std::thread *mAudioThread = nullptr;
    std::shared_ptr<AVPacketQueue> mAudioPacketQueue = nullptr;
    std::shared_ptr<AudioDecoder> mAudioDecoder = nullptr;

    void VideoDecodeLoop();

    void AudioDecodeLoop();

    bool readAvPacketToQueue(ReadPackType type);

    bool pushPacketToQueue(AVPacket *packet, const std::shared_ptr<AVPacketQueue> &queue) const;

    bool pushFrameToQueue(AVFrame *frame, const std::shared_ptr<AVFrameQueue> &queue, bool front);

    void ReadPacketLoop();

    void ReadVideoFrameLoop();

    void updatePlayerState(PlayerState state);

    void onPlayCompleted(JNIEnv *env);
};

#endif //VIDEOLEARN_FFMPEGPLAY_H
