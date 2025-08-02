#ifndef FFMPEGDEMO_AVPACKETQUEUE_H
#define FFMPEGDEMO_AVPACKETQUEUE_H

#include <queue>
#include <pthread.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "../include/libavutil/avutil.h"
}

class AVPacketQueue {

public:
    AVPacketQueue(int64_t maxSize);

    ~AVPacketQueue();

    void push(AVPacket *packet);

    AVPacket *pop(bool pop);

    bool checkLastIsEofPack();

    void clear();

    bool isFull();

    bool isFullWait();

    void checkEmptyWait();

    void checkNotEmptyWait();

    bool isEmpty();

    void wait(unsigned int timeOutMs = -1);

    void notify();
    void notify(bool release);

private:
    int64_t mMaxSize = 60;
    bool release = true;//queue有加锁等待，避免stop后释放不了锁，导致anr，添加变量控制是否需要加锁等待

    std::queue<AVPacket *> mQueue;

    pthread_cond_t mCond{};
    pthread_mutex_t mMutex{};
};


#endif //FFMPEGDEMO_AVPACKETQUEUE_H
