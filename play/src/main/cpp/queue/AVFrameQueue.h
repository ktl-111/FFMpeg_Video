#ifndef VIDEOLEARN_AVFRAMEQUEUE_H
#define VIDEOLEARN_AVFRAMEQUEUE_H

#include <queue>
#include <pthread.h>

extern "C" {
#include "../include/libavcodec/avcodec.h"
#include "../include/libavutil/avutil.h"
}

class AVFrameQueue {

public:
    AVFrameQueue(int64_t maxSize);

    ~AVFrameQueue();

    void push(AVFrame *frame);

    AVFrame* pop();

    int popTo(AVFrame *packet);

    void clear();

    bool isFull();

    bool isEmpty();

    void wait(unsigned int timeOutMs = -1);

    void notify();

private:
    int64_t mMaxSize = 60;

    std::queue<AVFrame *> mQueue;

    pthread_cond_t mCond{};
    pthread_mutex_t mMutex{};
};



#endif //VIDEOLEARN_AVFRAMEQUEUE_H
