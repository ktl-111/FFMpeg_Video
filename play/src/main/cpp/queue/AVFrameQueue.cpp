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
    LOGI("[AVFrameQueue] push pts:%ld %d", packet->pts, packet->format)
    mQueue.push(packet);
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVFrame *AVFrameQueue::pop() {
    pthread_mutex_lock(&mMutex);
    AVFrame *packet = mQueue.front();
    mQueue.pop();
    pthread_mutex_unlock(&mMutex);
    notify();
    return packet;
}

int AVFrameQueue::popTo(AVFrame *dstFrame) {
    pthread_mutex_lock(&mMutex);
    bool isEmpty = mQueue.empty() && mQueue.size() <= 0;
    if (isEmpty) {
        pthread_mutex_unlock(&mMutex);
        return -1;
    }
    AVFrame *frame = mQueue.front();
    if (frame == nullptr) {
        LOGE("[AVFrameQueue], popTo failed")
    }
    LOGI("[AVFrameQueue] popTo pts:%ld %d", frame->pts, frame->format)
    int ref = av_frame_ref(dstFrame, frame);
    if (ref != 0) {
        LOGE("[AVFrameQueue], popTo failed, ref: %d", ref);
    }

    av_frame_free(&frame);
    mQueue.pop();
    pthread_mutex_unlock(&mMutex);
    notify();
    return ref;
}

int AVFrameQueue::front(AVFrame *dstFrame) {
    pthread_mutex_lock(&mMutex);
    bool isEmpty = mQueue.empty() && mQueue.size() <= 0;
    if (isEmpty) {
        LOGE("[AVFrameQueue], front empty")
        pthread_mutex_unlock(&mMutex);
        return -1;
    }
    AVFrame *frame = mQueue.front();
    if (frame == nullptr) {
        LOGE("[AVFrameQueue], front failed")
    }

    int ref = av_frame_ref(dstFrame, frame);
    if (ref != 0) {
        LOGE("[AVFrameQueue], front failed, ref: %d %s", ref, av_err2str(ref));
    }
    pthread_mutex_unlock(&mMutex);
    return ref;
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
    LOGI("[AVFrameQueue], packet queue wait %d", timeOutMs)
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

int64_t AVFrameQueue::getSize() {
    int64_t size;
    pthread_mutex_lock(&mMutex);
    size = (int64_t) mQueue.size();
    pthread_mutex_unlock(&mMutex);
    return size;
}

void AVFrameQueue::checkEmptyWait() {
    LOGI("[AVFrameQueue] checkEmptyWait start")
    pthread_mutex_lock(&mMutex);
    int64_t size = (int64_t) mQueue.size();
    if (size <= 0) {
        LOGI("[AVFrameQueue] empty,wait")
        pthread_cond_wait(&mCond, &mMutex);
    }
    pthread_mutex_unlock(&mMutex);
    LOGI("[AVFrameQueue] checkEmptyWait end")
}

bool AVFrameQueue::isEmpty() {
    int64_t size;
    pthread_mutex_lock(&mMutex);
    size = (int64_t) mQueue.size();
    pthread_mutex_unlock(&mMutex);
    return size <= 0;
}

void AVFrameQueue::clear() {
    LOGI("[AVFrameQueue], clear queue,start")
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

int AVFrameQueue::getFrameByTime(AVFrame *dstFrame, int64_t time, bool isBack) {
    pthread_mutex_lock(&mMutex);
    int ret = -1;
    bool isPop = false;
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue] is empty")
        ret = AVERROR(EAGAIN);
    } else {
        while (!mQueue.empty()) {
            AVFrame *srcFrame = mQueue.front();
            int64_t pts = srcFrame->pts;
            pts *= av_q2d(srcFrame->time_base) * 1000;
            LOGI("[AVFrameQueue], getFrameByTime %ld %ld", pts, time)

            if (pts >= time) {
                ret = av_frame_ref(dstFrame, srcFrame);
                if (ret != 0 && srcFrame->data[0] != NULL) {
                    LOGI("[AVFrameQueue],memcpy ret:%d", ret)
                    memcpy(dstFrame->data, srcFrame->data, sizeof(srcFrame->data));
                }
//                av_frame_free(&srcFrame);
                break;
            } else {
                mQueue.pop();
                av_frame_free(&srcFrame);
                srcFrame = nullptr;
                isPop = true;
            }
        }
    }
    pthread_mutex_unlock(&mMutex);
    if (isPop) {
        notify();
    }
    return ret;
}
