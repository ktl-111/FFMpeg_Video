
#ifndef FFMPEGDEMO_MUTEXOBJ_H
#define FFMPEGDEMO_MUTEXOBJ_H

#include <pthread.h>

class MutexObj {

public:
    MutexObj();

    ~MutexObj();

    void wakeUp();

    void wait();

private:
    pthread_cond_t mCond{};
    pthread_mutex_t mMutex{};
};


#endif //FFMPEGDEMO_MUTEXOBJ_H
