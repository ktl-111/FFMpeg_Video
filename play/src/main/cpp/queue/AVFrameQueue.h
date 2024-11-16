#ifndef VIDEOLEARN_AVFRAMEQUEUE_H
#define VIDEOLEARN_AVFRAMEQUEUE_H

#include <queue>
#include <pthread.h>
#include <string>

extern "C" {
#include "../include/libavcodec/avcodec.h"
#include "../include/libavutil/avutil.h"
}

class AVFrameQueue {

public:
    AVFrameQueue(int64_t maxSize, std::string tag);

    ~AVFrameQueue();

    void pushBack(AVFrame *frame, bool noti);

    void pushFront(AVFrame *frame);

    bool checkLastIsEofFrame();

    void resetIndex();

    AVFrame *getFrame(bool pop, bool findBack);

    AVFrame *getFrameUnlock(bool pop, bool findBack);

    AVFrame *back();

    AVFrame *front();

    void clear(bool noti);

    bool isFull();

    void checkEmptyWait();

    bool isEmpty();

    int64_t getSize();

    void wait(unsigned int timeOutMs = -1);

    void notify();

    AVFrame *getFrameByTime(int64_t time, bool findBack);

    AVFrame *getFrameByIndex(int index);

    int64_t mMaxSize = 60;
private:
    std::deque<AVFrame *> mQueue;

    pthread_cond_t mCond{};
    pthread_mutex_t mMutex{};
    char *mTag = nullptr;
    int currIndex = -1;
};


#endif //VIDEOLEARN_AVFRAMEQUEUE_H
