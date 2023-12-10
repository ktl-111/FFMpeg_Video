#include "AVFrameQueue.h"
#include <ctime>
#include "../utils/loghelper.h"

extern "C" {
#include "libavutil/time.h"
}

AVFrameQueue::AVFrameQueue(int64_t maxSize) {
    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mCond, nullptr);
    mMaxSize = maxSize;
}

AVFrameQueue::~AVFrameQueue() {
    LOGI("~AVFrameQueue")
    clear();
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
}

void AVFrameQueue::push(AVFrame *packet) {
    pthread_mutex_lock(&mMutex);
    mQueue.push(packet);
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVFrame *AVFrameQueue::pop() {
    pthread_mutex_lock(&mMutex);
    AVFrame *packet = mQueue.front();
    mQueue.pop();
    pthread_mutex_unlock(&mMutex);
    return packet;
}

int AVFrameQueue::popTo(AVFrame *packet) {
    pthread_mutex_lock(&mMutex);
    bool isEmpty = mQueue.empty() && mQueue.size() <= 0;
    if (isEmpty) {
        pthread_mutex_unlock(&mMutex);
        return -1;
    }
    AVFrame *pkt = mQueue.front();
    if (pkt == nullptr) {
        LOGE("[AVFrameQueue], popTo failed")
    }

    int ref = av_frame_ref(packet, pkt);
    if (ref != 0) {
        LOGE("[AVFrameQueue], popTo failed, ref: %d", ref);
    }

    av_frame_free(&pkt);
    mQueue.pop();
    pthread_mutex_unlock(&mMutex);
    return 0;
}

bool AVFrameQueue::isFull() {
    int64_t queueSize;
    pthread_mutex_lock(&mMutex);
    queueSize = (int) mQueue.size();
    pthread_mutex_unlock(&mMutex);

    return queueSize >= mMaxSize;
}

void AVFrameQueue::wait(unsigned int timeOutMs) {
    pthread_mutex_lock(&mMutex);
    LOGI("packet queue wait %d", timeOutMs)
    if (timeOutMs > 0) {
        struct timespec abs_time{};
        struct timeval now_time{};
        gettimeofday(&now_time, nullptr);
        int n_sec = now_time.tv_usec * 1000 + (timeOutMs % 1000) * 1000000;
        abs_time.tv_nsec = n_sec % 1000000000;
        abs_time.tv_sec = now_time.tv_sec + n_sec / 1000000000 + timeOutMs / 1000;
        pthread_cond_timedwait(&mCond, &mMutex, &abs_time);
    } else {
        pthread_cond_wait(&mCond, &mMutex);
    }

    pthread_mutex_unlock(&mMutex);
}

void AVFrameQueue::notify() {
    pthread_mutex_lock(&mMutex);
    pthread_cond_signal(&mCond);
    pthread_mutex_unlock(&mMutex);
}

bool AVFrameQueue::isEmpty() {
    int64_t size;
    pthread_mutex_lock(&mMutex);
    size = (int64_t) mQueue.size();
    pthread_mutex_unlock(&mMutex);
    return size <= 0;
}

void AVFrameQueue::clear() {
    pthread_mutex_lock(&mMutex);
    int64_t size = mQueue.size();
    LOGI("[AVFrameQueue], clear queue, size: %" PRId64, size)
    while (!mQueue.empty() && size > 0) {
        AVFrame *packet = mQueue.front();
        if (packet != nullptr) {
            av_frame_free(&packet);
        }
        mQueue.pop();
    }
    pthread_mutex_unlock(&mMutex);
    notify();
}

