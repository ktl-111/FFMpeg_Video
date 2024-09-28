#include "AVFrameQueue.h"
#include <ctime>
#include "Logger.h"

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

void AVFrameQueue::pushBack(AVFrame *frame) {
    pthread_mutex_lock(&mMutex);
    LOGI("[AVFrameQueue] pushBack pts:%ld %d", frame->pts, frame->format)
    mQueue.push_back(frame);
    pthread_mutex_unlock(&mMutex);
    notify();
}

void AVFrameQueue::pushFront(AVFrame *frame) {
    pthread_mutex_lock(&mMutex);
    LOGI("[AVFrameQueue] pushFront pts:%ld %d", frame->pts, frame->format)
    mQueue.push_front(frame);
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVFrame *AVFrameQueue::popFront() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue] popFront is empty")
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    AVFrame *frame = mQueue.front();
    mQueue.pop_front();
    pthread_mutex_unlock(&mMutex);
    notify();
    return frame;
}

AVFrame *AVFrameQueue::back() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue] back is empty")
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    AVFrame *frame = mQueue.back();
    pthread_mutex_unlock(&mMutex);
    notify();
    return frame;
}

AVFrame *AVFrameQueue::front() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue] front is empty")
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    AVFrame *frame = mQueue.front();
    pthread_mutex_unlock(&mMutex);
    notify();
    return frame;
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
    pthread_cond_broadcast(&mCond);
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

bool AVFrameQueue::checkLastIsEofFrame() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue],checkLastIsEofPack isEmpty")
        pthread_mutex_unlock(&mMutex);
        return false;
    }
    AVFrame *pFrame = mQueue.back();
    bool isEof = pFrame->pkt_size == 0;
    LOGI("[AVFrameQueue],checkLastIsEofPack isEof:%d", isEof)
    pthread_mutex_unlock(&mMutex);
    return isEof;
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
        mQueue.pop_front();
    }
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVFrame *AVFrameQueue::getFrameByTime(int64_t time) {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue],getFrameByTime is empty")
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    bool isPop = false;
    AVFrame *preFrame = nullptr;
    bool freePreFrame = true;
    AVFrame *finalFrame = nullptr;
    while (!mQueue.empty()) {
        //获取pts在time之间的frame
        AVFrame *srcFrame = mQueue.front();
        int64_t pts = srcFrame->pts;
        pts *= av_q2d(srcFrame->time_base) * 1000;
        if (preFrame != nullptr) {
            LOGI("[AVFrameQueue],getFrameByTime while %ld %ld preFrame:%ld(%p)", pts, time,
                 preFrame->pts, preFrame)
        } else {
            LOGI("[AVFrameQueue],getFrameByTime while %ld %ld ", pts, time)
        }

        if (pts >= time) {
            if (preFrame != nullptr) {
                freePreFrame = false;
                mQueue.push_front(preFrame);
                finalFrame = preFrame;
            } else {
                finalFrame = srcFrame;
            }
            pts = finalFrame->pts * (av_q2d(finalFrame->time_base) * 1000);
            LOGI("[AVFrameQueue],getFrameByTime success pts:%ld", pts)
            break;
        } else {
            if (preFrame != nullptr) {
                av_frame_free(&preFrame);
                preFrame = nullptr;
            }
            isPop = true;
            mQueue.pop_front();
            preFrame = srcFrame;
            LOGI("[AVFrameQueue],getFrameByTime not find srcFrame:%p", srcFrame)

        }
    }
    if (freePreFrame && preFrame != nullptr) {
        LOGI("pushFrameToQueue freePreFrame %p", preFrame)
        av_frame_free(&preFrame);
        preFrame = nullptr;
    }
    LOGI("[AVFrameQueue],getFrameByTime done")
    pthread_mutex_unlock(&mMutex);
    if (isPop) {
        notify();
    }
    return finalFrame;
}
