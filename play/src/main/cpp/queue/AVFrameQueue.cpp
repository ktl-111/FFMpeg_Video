#include "AVFrameQueue.h"
#include <ctime>
#include "Logger.h"

extern "C" {
#include "libavutil/time.h"
}

AVFrameQueue::AVFrameQueue(int64_t maxSize, std::string tag) {
    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mCond, nullptr);
    mMaxSize = maxSize;
    if (mTag) {
        free(mTag);
    }
    mTag = strdup(tag.c_str());
    LOGI("AVFrameQueue(%s) mMaxSize:%ld", mTag, maxSize)
}

AVFrameQueue::~AVFrameQueue() {
    LOGI("~AVFrameQueue(%s)", mTag)
    clear(true);
    delete[] mTag;
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
}

void AVFrameQueue::resetIndex() {
    pthread_mutex_lock(&mMutex);
    currIndex = -1;
    pthread_mutex_unlock(&mMutex);
}

void AVFrameQueue::pushBack(AVFrame *frame, bool noti) {
    pthread_mutex_lock(&mMutex);
    LOGI("[AVFrameQueue(%s)] pushBack pts:%ld(%f) size:%ld index:%d", mTag, frame->pts,
         frame->pts * av_q2d(frame->time_base), mQueue.size(),
         currIndex)
    mQueue.push_back(frame);
    pthread_mutex_unlock(&mMutex);
    if (noti) {
        notify();
    }
}

void AVFrameQueue::pushFront(AVFrame *frame) {
    pthread_mutex_lock(&mMutex);
    LOGI("[AVFrameQueue(%s)] pushFront pts:%ld(%f) %d", mTag, frame->pts,
         frame->pts * av_q2d(frame->time_base), frame->format)
    mQueue.push_front(frame);
    currIndex++;
    pthread_mutex_unlock(&mMutex);
    notify();
}

AVFrame *AVFrameQueue::getFrameUnlock(bool pop, bool findBack) {
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue(%s)] getFrame is empty", mTag)
        return nullptr;
    }
    bool tempPop = pop;
    unsigned long size = mQueue.size();
    bool isFull = size == mMaxSize;
    if (findBack) {
        currIndex--;
    } else {
        currIndex++;
    }
    size = mQueue.size();
    if (currIndex < 0) {
        LOGI("[AVFrameQueue(%s)] getFrame currIndex < 0", mTag)
        currIndex = 0;
        return nullptr;
    } else {
        if (currIndex >= size) {
            currIndex = size - 1;
            LOGI("[AVFrameQueue(%s)] getFrame currIndex > size", mTag)
            return nullptr;
        }
    }
    AVFrame *frame = mQueue.at(currIndex);

    if (!pop && (isFull && currIndex > mMaxSize / 2)) {
        pop = true;
    }
    if (pop) {
        if (!tempPop) {
            AVFrame *pFrame = mQueue.front();
            av_frame_free(&pFrame);
        }
        mQueue.pop_front();
        currIndex--;
    }

    LOGI("[AVFrameQueue(%s)] getFrame end pts:%ld index:%d size:%ld isFull:%d pop:%d", mTag,
         frame->pts,
         currIndex, mQueue.size(), isFull, pop)
    return frame;
}

AVFrame *AVFrameQueue::getFrame(bool pop, bool findBack) {
    pthread_mutex_lock(&mMutex);
    AVFrame *frame = getFrameUnlock(pop, findBack);
    pthread_mutex_unlock(&mMutex);
    notify();
    return frame;
}

AVFrame *AVFrameQueue::back() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue(%s)] back is empty", mTag)
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
        LOGI("[AVFrameQueue(%s)] front is empty", mTag)
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
    LOGI("[AVFrameQueue(%s)], frame queue wait %d", mTag, timeOutMs)
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
    LOGI("[AVFrameQueue(%s)] checkEmptyWait start", mTag)
    pthread_mutex_lock(&mMutex);
    int64_t size = (int64_t) mQueue.size();
    if (size <= 0) {
        LOGI("[AVFrameQueue(%s)] empty,wait", mTag)
        pthread_cond_wait(&mCond, &mMutex);
    }
    pthread_mutex_unlock(&mMutex);
    LOGI("[AVFrameQueue(%s)] checkEmptyWait end", mTag)
}

bool AVFrameQueue::checkLastIsEofFrame() {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue(%s)],checkLastIsEofPack isEmpty", mTag)
        pthread_mutex_unlock(&mMutex);
        return false;
    }
    AVFrame *pFrame = mQueue.back();
    bool isEof = pFrame->pkt_size == 0;
    LOGI("[AVFrameQueue(%s)],checkLastIsEofPack isEof:%d", mTag, isEof)
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

void AVFrameQueue::clear(bool noti) {
    LOGI("[AVFrameQueue(%s)], clear queue,start", mTag)
    pthread_mutex_lock(&mMutex);
    int64_t size = mQueue.size();
    LOGI("[AVFrameQueue(%s)], clear queue, size: %" PRId64, mTag, size)
    while (!mQueue.empty() && size > 0) {
        AVFrame *frame = mQueue.front();
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
        mQueue.pop_front();
    }
    currIndex = -1;
    pthread_mutex_unlock(&mMutex);
    if (noti) {
        notify();
    }
}

AVFrame *AVFrameQueue::getFrameByTime(int64_t time, bool findBack) {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue(%s)],getFrameByTime is empty", mTag)
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    bool find = false;
    if (currIndex == -1) {
        //clear后,index=-1,从头开始find
        findBack = false;
    } else {
        AVFrame *pFrame = mQueue.at(currIndex);
        if (pFrame->pkt_size == 0) {
            findBack = true;
        } else {
            if (pFrame->pts * av_q2d(pFrame->time_base) * 1000 > time) {
                findBack = true;
            } else {
                findBack = false;
            }
        }
    }
    AVFrame *srcFrame = nullptr;
    while (!mQueue.empty()) {
        //获取pts在time之间的frame
        srcFrame = getFrameUnlock(false, findBack);
        if (srcFrame == nullptr) {
            LOGI("[AVFrameQueue(%s)],getFrameByTime is null", mTag)
            break;
        }
        if (srcFrame->pkt_size == 0) {
            findBack = true;
            LOGI("[AVFrameQueue(%s)],getFrameByTime is eof", mTag)
            continue;
        }
        int64_t pts = srcFrame->pts * av_q2d(srcFrame->time_base) * 1000;

        if (pts == time) {
            find = true;
        }
        if (find) {
            pts = srcFrame->pts * (av_q2d(srcFrame->time_base) * 1000);
            LOGI("[AVFrameQueue(%s)],getFrameByTime success pts:%ld", mTag, pts)
            break;
        }
        if (findBack) {
            if (pts < time) {
                find = true;
                findBack = false;
            }
        } else {
            if (pts > time) {
                find = true;
                findBack = true;
            }
        }
        LOGI("[AVFrameQueue(%s)],getFrameByTime fore pts:%ld findBack:%d", mTag, pts, findBack)
    }
    LOGI("[AVFrameQueue(%s)],getFrameByTime done", mTag)
    pthread_mutex_unlock(&mMutex);

    return srcFrame;
}

AVFrame *AVFrameQueue::getFrameByIndex(int index) {
    pthread_mutex_lock(&mMutex);
    if (mQueue.empty()) {
        LOGI("[AVFrameQueue(%s)],getFrameByIndex is empty", mTag)
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    unsigned long size = mQueue.size();
    if (index < 0 || index >= size) {
        LOGI("[AVFrameQueue(%s)],getFrameByIndex fail,index:%d,size:%ld", mTag, index, size)
        pthread_mutex_unlock(&mMutex);
        return nullptr;
    }
    AVFrame *srcFrame = mQueue.at(index);

    LOGI("[AVFrameQueue(%s)],getFrameByIndex %d done", mTag, index)
    pthread_mutex_unlock(&mMutex);

    return srcFrame;
}
