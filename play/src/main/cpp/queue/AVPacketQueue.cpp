#include "AVPacketQueue.h"
#include <ctime>
#include "Logger.h"

AVPacketQueue::AVPacketQueue(int64_t maxSize) {
    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mCond, nullptr);
    mMaxSize = maxSize;
}

AVPacketQueue::~AVPacketQueue() {
    LOGI("~AVPacketQueue")
    clear();
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
}

void AVPacketQueue::push(AVPacket *packet) {
    pthread_mutex_lock(&mMutex);
    mQueue.push(packet);
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVPacket *AVPacketQueue::pop() {
    pthread_mutex_lock(&mMutex);
    bool isEmpty = mQueue.empty() && mQueue.size() <= 0;
    if (isEmpty) {
        LOGI("[AVPacketQueue],popFront isEmpty")
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    AVPacket *packet = mQueue.front();
    mQueue.pop();
    pthread_mutex_unlock(&mMutex);
    notify();
    return packet;
}

bool AVPacketQueue::isFull() {
    int64_t queueSize;
    pthread_mutex_lock(&mMutex);
    queueSize = (int) mQueue.size();
    pthread_mutex_unlock(&mMutex);

    return queueSize >= mMaxSize;
}

void AVPacketQueue::wait(unsigned int timeOutMs) {
    pthread_mutex_lock(&mMutex);
    LOGI("[AVPacketQueue], packet queue wait start,timeoutms:%d", timeOutMs)
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
    LOGI("[AVPacketQueue], packet queue wait end")
    pthread_mutex_unlock(&mMutex);
}

void AVPacketQueue::notify() {
    pthread_mutex_lock(&mMutex);
    pthread_cond_broadcast(&mCond);
    pthread_mutex_unlock(&mMutex);
}

void AVPacketQueue::checkEmptyWait() {
    LOGI("[AVPacketQueue] checkEmptyWait start")
    pthread_mutex_lock(&mMutex);
    int64_t size = (int64_t) mQueue.size();
    if (size <= 0) {
        LOGI("[AVPacketQueue] empty,wait")
        pthread_cond_wait(&mCond, &mMutex);
    }
    pthread_mutex_unlock(&mMutex);
    LOGI("[AVPacketQueue] checkEmptyWait end")
}

void AVPacketQueue::checkNotEmptyWait() {
    LOGI("[AVPacketQueue] checkEmptyWait start")
    pthread_mutex_lock(&mMutex);
    int64_t size = (int64_t) mQueue.size();
    if (size != 0) {
        LOGI("[AVPacketQueue] not empty,wait")
        pthread_cond_wait(&mCond, &mMutex);
    }
    pthread_mutex_unlock(&mMutex);
    LOGI("[AVPacketQueue] checkEmptyWait end")
}

bool AVPacketQueue::checkLastIsEofPack() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVPacketQueue],checkLastIsEofPack isEmpty")
        pthread_mutex_unlock(&mMutex);
        return false;
    }
    AVPacket *pPacket = mQueue.back();
    bool isEof = pPacket->size == 0 && pPacket->data == nullptr;
    LOGI("[AVPacketQueue],checkLastIsEofPack isEof:%d", isEof)
    pthread_mutex_unlock(&mMutex);
    return isEof;
}

bool AVPacketQueue::isEmpty() {
    int64_t size;
    LOGI("[AVPacketQueue] isEmpty check start")
    pthread_mutex_lock(&mMutex);
    size = (int64_t) mQueue.size();
    pthread_mutex_unlock(&mMutex);
    LOGI("[AVPacketQueue] isEmpty check end")
    return size <= 0;
}

void AVPacketQueue::clear() {
    pthread_mutex_lock(&mMutex);
    int64_t size = mQueue.size();
    LOGI("[AVPacketQueue], clear queue, size: %" PRId64, size)
    while (!mQueue.empty() && size > 0) {
        AVPacket *packet = mQueue.front();
        if (packet != nullptr) {
            av_packet_free(&packet);
            av_freep(&packet);
        }
        mQueue.pop();
    }
    pthread_mutex_unlock(&mMutex);
    notify();
}

