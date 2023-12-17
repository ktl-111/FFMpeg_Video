//
// Created by Administrator on 2023/11/19.
//
#include "FFMpegPlayer.h"

FFMpegPlayer::FFMpegPlayer() {
    mMutexObj = std::make_shared<MutexObj>();
    LOGI("FFMpegPlayer")
}

FFMpegPlayer::~FFMpegPlayer() {
    mPlayerJni.reset();
    LOGI("~FFMpegPlayer")
}

void FFMpegPlayer::init(JNIEnv *env, jobject thiz) {
    jclass jclazz = env->GetObjectClass(thiz);
    if (jclazz == nullptr) {
        return;
    }
    LOGI("FFMpegPlayer init")
    mPlayerJni.reset();
    mPlayerJni.instance = env->NewGlobalRef(thiz);
    mPlayerJni.onVideoConfig = env->GetMethodID(jclazz, "onNativeVideoConfig", "(IIDD)V");
    mPlayerJni.onPlayProgress = env->GetMethodID(jclazz, "onNativePalyProgress", "(D)V");
    mPlayerJni.onPlayCompleted = env->GetMethodID(jclazz, "onNativePalyComplete", "()V");
}

bool resultIsFail(int result) {
    return result < 0;
}

bool FFMpegPlayer::prepare(JNIEnv *env, std::string &path, jobject surface, jobject out_config) {
    if (mJvm == nullptr) {
        env->GetJavaVM(&mJvm);
    }
    // 设置JavaVM，否则无法进行硬解码
    av_jni_set_java_vm(mJvm, nullptr);
    //分配 mAvFormatContext
    mAvFormatContext = avformat_alloc_context();

    //打开文件输入流
    int result = avformat_open_input(&mAvFormatContext, path.c_str(), nullptr, nullptr);
    if (resultIsFail(result)) {
        LOGE("avformat_open_input fail,result:%d", result)
        return false;
    }

    //提取输入文件中的数据流信息
    result = avformat_find_stream_info(mAvFormatContext, nullptr);
    if (resultIsFail(result)) {
        LOGE("avformat_find_stream_info fail,result:%d", result)
        return false;
    }

    bool audioPrePared = false;
    bool videoPrepared = false;
    for (int i = 0; i < mAvFormatContext->nb_streams; ++i) {
        AVStream *pStream = mAvFormatContext->streams[i];
        AVCodecParameters *codecpar = pStream->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            std::shared_ptr<OutConfig> outConfig;
            if (out_config) {
                jclass outConfigClass = env->GetObjectClass(out_config);
                jfieldID outWidthId = env->GetFieldID(outConfigClass, "width", "I");
                jint outWidth = env->GetIntField(out_config, outWidthId);

                jfieldID outHeightId = env->GetFieldID(outConfigClass, "height", "I");
                jint outHeight = env->GetIntField(out_config, outHeightId);

                jfieldID outFpsId = env->GetFieldID(outConfigClass, "fps", "D");
                jdouble outFps = env->GetDoubleField(out_config, outFpsId);

                outConfig = std::make_shared<OutConfig>(outWidth, outHeight, outFps);

                LOGI("set out config,width:%d,height:%d,fps:%f", outWidth, outHeight, outFps)
            } else {
                LOGE("not out config")
            }
            //视频流
            LOGI("video stream,index:%d result:%d", i, mPlayerJni.isValid())
            mVideoDecoder = std::make_shared<VideoDecoder>(i, mAvFormatContext);
            int frameSize = 50;
//            if (outConfig) {
//                mVideoDecoder->setOutConfig(outConfig);
//                frameSize = outConfig->getFps() * 5;
//            }

            mVideoPacketQueue = std::make_shared<AVPacketQueue>(50);
            mVideoFrameQueue = std::make_shared<AVFrameQueue>(frameSize);
            mVideoThread = new std::thread(&FFMpegPlayer::VideoDecodeLoop, this);
            mVideoDecodeThread = new std::thread(&FFMpegPlayer::ReadVideoFrameLoop, this);

            mVideoDecoder->setSurface(surface);
            videoPrepared = mVideoDecoder->prepare(env);
            if (surface != nullptr && !videoPrepared) {
                mVideoDecoder->release();
                LOGE("[video] hw decoder prepare failed, fallback to software decoder")
                mVideoDecoder->setSurface(nullptr);
                videoPrepared = mVideoDecoder->prepare(env);
            }

            if (mPlayerJni.isValid()) {
                env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onVideoConfig,
                                    mVideoDecoder->getWidth(), mVideoDecoder->getHeight(),
                                    mVideoDecoder->getDuration(), mVideoDecoder->getFps());
            }
        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频流
            LOGI("audio stream,index:%d", i)
            mAudioDecoder = std::make_shared<AudioDecoder>(i, mAvFormatContext);
            audioPrePared = mAudioDecoder->prepare(env);
            if (false) {
                mAudioPacketQueue = std::make_shared<AVPacketQueue>(50);
                mAudioThread = new std::thread(&FFMpegPlayer::AudioDecodeLoop, this);
                mAudioDecoder->setErrorMsgListener([](int err, std::string &msg) {
                    LOGE("[audio] err code: %d, msg: %s", err, msg.c_str())
                });
            } else {
                mAudioDecoder = nullptr;
                mAudioPacketQueue = nullptr;
                mAudioThread = nullptr;
                LOGE("audio track prepared failed!!!")
            }
        }
    }
    bool prepared = videoPrepared || audioPrePared;
    LOGI("videoPrepared: %d, audioPrePared: %d, path: %s", videoPrepared, audioPrePared,
         path.c_str())
    if (prepared) {
        updatePlayerState(PlayerState::PREPARE);
    }
    return prepared;

}

void FFMpegPlayer::start() {
    LOGI("FFMpegPlayer::start, state: %d", mPlayerState)
    if (mPlayerState != PlayerState::PREPARE) {  // prepared failed
        return;
    }
    updatePlayerState(PlayerState::START);
    if (mReadPacketThread == nullptr) {
        mReadPacketThread = new std::thread(&FFMpegPlayer::ReadPacketLoop, this);
    }

}

bool FFMpegPlayer::seekTo(double seekTime) {
    pause();
    if (preSeekTime == -1) {
        LOGI("seek pre is 0,%ld,%ld", mVideoDecoder->getTimestamp(),
             mVideoDecoder->getTimestamp() / 1000)
        preSeekTime = mVideoDecoder->getTimestamp() / 1000;
    }
    isSeek = true;
    int64_t currPlayTime = mVideoDecoder->getTimestamp();
    double lastCacheFrameTime =
            currPlayTime + (mVideoFrameQueue->isEmpty() ? 0 : mVideoFrameQueue->getSize()) *
                           (1000.0f / mVideoDecoder->getFps());
    bool isBack = preSeekTime > seekTime;
    LOGI("seek seekTime:%lf,lastCache:%lf,preSeekTime:%lf", seekTime, lastCacheFrameTime,
         preSeekTime)
    if (seekTime * 1000 > lastCacheFrameTime || isBack) {
        bool callSeek = true;
        bool callQueueClear = true;
        if (isBack) {
            AVFrame *frontFrame = av_frame_alloc();
            if (mVideoFrameQueue->front(frontFrame)) {
                LOGI("seek front pts:%ld(%lf)", frontFrame->pts,
                     frontFrame->pts * av_q2d(frontFrame->time_base))
                if (frontFrame->pts * av_q2d(frontFrame->time_base) > seekTime) {
                    callSeek = false;
                    callQueueClear = false;
                }
                av_frame_free(&frontFrame);
            }
        }
        if (callSeek) {
            mVideoDecoder->seek(seekTime);
        }
        if (callQueueClear) {
            mVideoPacketQueue->clear();
            mVideoFrameQueue->clear();
        }
    } else {
        mVideoFrameQueue->notify();
        mVideoPacketQueue->notify();
    }
    preSeekTime = seekTime;
    if(isBack){
        mVideoPacketQueue->wait();
    }
    AVFrame *pFrame = av_frame_alloc();

    int ret = mVideoFrameQueue->getFrameByTime(pFrame, seekTime, isBack);
    if (ret != 0) {
        LOGE("seek %lf get frame fail %d", seekTime, ret)
        av_frame_free(&pFrame);
        pFrame = nullptr;
        if (ret == AVERROR(EAGAIN)) {
            LOGI("seek %lf start", seekTime)
            mVideoFrameQueue->wait();
            LOGI("seek %lf end", seekTime)
            return seekTo(seekTime);
        }
        return false;
    }
    double frameTime = pFrame->pts * av_q2d(pFrame->time_base);
    if (seekTime > frameTime) {
        LOGI("seek %lf done,show %lf,continue seek", seekTime, frameTime)
        av_frame_free(&pFrame);
        return seekTo(seekTime);
    }
    LOGI("seek %lf done,show %lf", seekTime, frameTime)
    mVideoDecoder->resultCallback(pFrame);
    LOGI("seek %lf done", seekTime)
    return true;
}

void FFMpegPlayer::stop() {
    LOGI("FFMpegPlayer::stop")
    // wakeup read packet thread and release it
    updatePlayerState(PlayerState::STOP);
    mMutexObj->wakeUp();
    if (mReadPacketThread != nullptr) {
        LOGE("join read thread")
        mReadPacketThread->join();
        delete mReadPacketThread;
        mReadPacketThread = nullptr;
        LOGE("release read thread")
    }
    if (mVideoDecodeThread != nullptr) {
        LOGE("join read thread")
        mVideoDecodeThread->join();
        delete mVideoDecodeThread;
        mVideoDecodeThread = nullptr;
        LOGE("release read thread")
    }

    mHasAbort = true;
    mIsMute = false;

    // release video res
    if (mVideoThread != nullptr) {
        LOGE("join video thread")
        if (mVideoPacketQueue) {
            mVideoPacketQueue->clear();
        }
        mVideoThread->join();
        delete mVideoThread;
        mVideoThread = nullptr;
    }
    mVideoDecoder = nullptr;
    LOGE("release video res")

    // release audio res
    if (mAudioThread != nullptr) {
        LOGE("join audio thread")
        if (mAudioPacketQueue) {
            mAudioPacketQueue->clear();
        }
        mAudioThread->join();
        delete mAudioThread;
        mAudioThread = nullptr;
    }
    mAudioDecoder = nullptr;
    LOGE("release audio res")

    if (mAvFormatContext != nullptr) {
        avformat_close_input(&mAvFormatContext);
        avformat_free_context(mAvFormatContext);
        mAvFormatContext = nullptr;
        LOGE("format context...release")
    }
}

void FFMpegPlayer::resume() {
    isSeek = false;
    updatePlayerState(PlayerState::PLAYING);
    if (mVideoDecoder) {
        mVideoDecoder->needFixStartTime();
    }
    if (mAudioDecoder) {
        mAudioDecoder->needFixStartTime();
    }
    mMutexObj->wakeUp();
    if (mAudioPacketQueue) {
        mAudioPacketQueue->notify();
    }
    mVideoPacketQueue->notify();
    mVideoFrameQueue->notify();
}

void FFMpegPlayer::pause() {
    updatePlayerState(PlayerState::PAUSE);
}


void FFMpegPlayer::VideoDecodeLoop() {
    if (mVideoDecoder == nullptr || mVideoPacketQueue == nullptr) {
        return;
    }

    mVideoDecoder->setOnFrameArrived([this](AVFrame *frame) {
        JNIEnv *env = nullptr;
        bool needAttach = mJvm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_EDETACHED;
        if (needAttach) {
            mJvm->AttachCurrentThread(&env, nullptr);
            LOGE("[video] AttachCurrentThread")
        }
        LOGI("OnFrameArrived start needAttach:%d", needAttach)
        if (!mHasAbort && mVideoDecoder) {
            if (mAudioDecoder) {
                auto diff = mAudioDecoder->getTimestamp() - mVideoDecoder->getTimestamp();
                LOGW("[video] frame arrived, AV time diff: %ld,isSeek: %d", diff, isSeek)
            }
            if (!isSeek) {
                int64_t timestamp = mVideoDecoder->getTimestamp();
                LOGI("avSync start %ld,isSeek: %d", timestamp, isSeek)
                mVideoDecoder->avSync(frame);
                LOGI("avSync end %ld,isSeek: %d", timestamp, isSeek)
                if (isSeek) {
                    return;
                }
            }
            mVideoDecoder->showFrameToWindow(frame);
            LOGI("async done")
            if (!isSeek && !mAudioDecoder && mPlayerJni.isValid()) { // no audio track
                double timestamp = mVideoDecoder->getTimestamp();
                env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onPlayProgress, timestamp);
            }
            if (needAttach) {
                mJvm->DetachCurrentThread();
            }
        } else {
            LOGE("[video] setOnFrameArrived, has abort")
        }
    });

    while (true) {
        while (!mHasAbort && mVideoFrameQueue->isEmpty()) {
            LOGE("[video] VideoDecodeLoop no frame, wait...")
            mVideoFrameQueue->wait();
        }

        if (mPlayerState == PlayerState::PAUSE) {
            LOGI("[video] VideoDecodeLoop decode pause wait")
            mMutexObj->wait();
            LOGI("[video] VideoDecodeLoop decode pause wakup state:%d", mPlayerState);
        }

        if (mHasAbort) {
            LOGE("[video] VideoDecodeLoop has abort...")
            break;
        }

        AVFrame *frame = av_frame_alloc();
        if (frame != nullptr) {
            int ret = mVideoFrameQueue->popTo(frame);
            if (ret == 0) {
                LOGI("[video] VideoDecodeLoop %ld", frame->pts)
                mVideoDecoder->resultCallback(frame);
                av_frame_free(&frame);
            } else {
                LOGE("VideoDecodeLoop pop frame failed...")
            }
        }
    }

    mVideoFrameQueue->clear();
    mVideoFrameQueue = nullptr;
    LOGE("[video] DetachCurrentThread");
}

void FFMpegPlayer::AudioDecodeLoop() {
    if (mAudioDecoder == nullptr || mAudioPacketQueue == nullptr) {
        return;
    }

    JNIEnv *env = nullptr;
    if (mJvm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_EDETACHED) {
        mJvm->AttachCurrentThread(&env, nullptr);
        LOGE("[audio] AttachCurrentThread")
    }

    mAudioDecoder->setOnFrameArrived([this, env](AVFrame *frame) {
        if (!mHasAbort && mAudioDecoder) {
            mAudioDecoder->avSync(frame);
            mAudioDecoder->playAudio(frame);
            if (mPlayerJni.isValid()) {
                double timestamp = mAudioDecoder->getTimestamp();
                env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onPlayProgress, timestamp);
            }
        } else {
            LOGE("[audio] setOnFrameArrived, has abort")
        }
    });

    while (true) {

        while (!mHasAbort && mAudioPacketQueue->isEmpty()) {
            LOGE("[audio] no packet, wait...")
            mAudioPacketQueue->wait();
        }
        if (mPlayerState == PlayerState::PAUSE) {
            LOGI("[audio] decode pause wait")
            mMutexObj->wait();
            LOGI("[audio] decode pause wakup state:%d", mPlayerState);
        }
        if (mHasAbort) {
            LOGE("[audio] has abort...")
            break;
        }

        AVPacket *packet = av_packet_alloc();
        if (packet != nullptr) {
            int ret = mAudioPacketQueue->popTo(packet);
            if (ret == 0) {
                do {
                    ret = mAudioDecoder->decode(packet, NULL);
                } while (mAudioDecoder->isNeedResent());

                av_packet_unref(packet);
                av_packet_free(&packet);
                if (ret == AVERROR_EOF) {
                    LOGE("AudioDecodeLoop AVERROR_EOF")
                    onPlayCompleted(env);
                }
            } else {
                LOGE("AudioDecodeLoop pop packet failed...")
            }
        }
    }

    mAudioPacketQueue->clear();
    mAudioPacketQueue = nullptr;

    mJvm->DetachCurrentThread();
    LOGE("[audio] DetachCurrentThread");
}

void FFMpegPlayer::ReadVideoFrameLoop() {
    while (true) {
        while (!mHasAbort && mVideoPacketQueue->isEmpty()) {
            LOGE("[video] ReadVideoFrameLoop no packet, wait...")
            mVideoPacketQueue->wait();
        }

        if (mHasAbort) {
            LOGE("[video] ReadVideoFrameLoop has abort...")
            break;
        }

        int decodeResult;
        do {
            decodeResult = -1;
            AVPacket *packet = av_packet_alloc();
            int ret = mVideoPacketQueue->popTo(packet);
            if (ret == 0) {
                AVFrame *pFrame = av_frame_alloc();
                do {
                    decodeResult = mVideoDecoder->decode(packet, pFrame);
                } while (mVideoDecoder->isNeedResent());

                av_packet_unref(packet);
                av_packet_free(&packet);
                LOGI("[video] ReadVideoFrameLoop decode %d", decodeResult)
                if (decodeResult == 0) {
                    if (mVideoFrameQueue->isEmpty()) {
                        mVideoDecoder->updateTimestamp(pFrame);
                    }
                    pushFrameToQueue(pFrame, mVideoFrameQueue);
                } else {
                    av_frame_free(&pFrame);
                }
            } else {
                LOGE("ReadVideoFrameLoop pop packet failed...")
            }
        } while (decodeResult == AVERROR(EAGAIN));
    }

    mVideoPacketQueue->clear();
    mVideoPacketQueue = nullptr;

    mJvm->DetachCurrentThread();
    LOGE("[video] DetachCurrentThread")
}

void FFMpegPlayer::ReadPacketLoop() {
    LOGI("FFMpegPlayer::ReadPacketLoop start")
    while (mPlayerState != PlayerState::STOP) {
        bool isEnd = readAvPacketToQueue(ReadPackType::ANY) != 0;
        if (isEnd) {
            LOGW("read av packet end, mPlayerState: %d", mPlayerState)
            break;
        }
    }
    LOGI("FFMpegPlayer::ReadPacketLoop end")
}

int FFMpegPlayer::readAvPacketToQueue(ReadPackType type) {
    reread:
    AVPacket *avPacket = av_packet_alloc();
    int ret = av_read_frame(mAvFormatContext, avPacket);
    bool suc = false;
    if (ret == 0) {
        if (type == ReadPackType::VIDEO) {
            if (avPacket->stream_index != mVideoDecoder->getStreamIndex()) {
                av_packet_free(&avPacket);
                av_freep(&avPacket);
                goto reread;
            }
        } else if (type == ReadPackType::AUDIO) {
            if (avPacket->stream_index != mAudioDecoder->getStreamIndex()) {
                av_packet_free(&avPacket);
                av_freep(&avPacket);
                goto reread;
            }
        }
        if (mVideoDecoder && mVideoPacketQueue &&
            avPacket->stream_index == mVideoDecoder->getStreamIndex()) {
            LOGI("push video pts:%ld(%f)", avPacket->pts,
                 avPacket->pts * av_q2d(mVideoDecoder->getTimeBase()))
            suc = pushPacketToQueue(avPacket, mVideoPacketQueue);
        } else if (mAudioDecoder && mAudioPacketQueue &&
                   avPacket->stream_index == mAudioDecoder->getStreamIndex()) {
            LOGI("push audio")
            suc = pushPacketToQueue(avPacket, mAudioPacketQueue);
        }
    } else {
        // send flush packet
        AVPacket *videoFlushPkt = av_packet_alloc();
        videoFlushPkt->size = 0;
        videoFlushPkt->data = nullptr;
        if (!pushPacketToQueue(videoFlushPkt, mVideoPacketQueue)) {
            av_packet_free(&videoFlushPkt);
            av_freep(&videoFlushPkt);
        }

        AVPacket *audioFlushPkt = av_packet_alloc();
        audioFlushPkt->size = 0;
        audioFlushPkt->data = nullptr;
        if (!pushPacketToQueue(audioFlushPkt, mAudioPacketQueue)) {
            av_packet_free(&audioFlushPkt);
            av_freep(&audioFlushPkt);
        }
        LOGE("read packet...end or failed: %d", ret)
        ret = -1;
    }

    if (!suc) {
        LOGI("av_read_frame, other...pts: %" PRId64 ", index: %d", avPacket->pts,
             avPacket->stream_index)
        av_packet_free(&avPacket);
        av_freep(&avPacket);
    }
    return ret;
}


bool FFMpegPlayer::pushPacketToQueue(AVPacket *packet,
                                     const std::shared_ptr<AVPacketQueue> &queue) const {
    if (queue == nullptr) {
        return false;
    }

    bool suc = false;
    while (queue->isFull()) {
        queue->wait();
        LOGD("queue is full, wait 10ms, packet index: %d", packet->stream_index)
    }
    queue->push(packet);
    suc = true;
    return suc;
}

bool FFMpegPlayer::pushFrameToQueue(AVFrame *frame,
                                    const std::shared_ptr<AVFrameQueue> &queue) const {
    if (queue == nullptr) {
        return false;
    }

    bool suc = false;
    while (queue->isFull()) {
        LOGD("pushFrameToQueue is full, wait start")
        queue->wait();
        LOGD("pushFrameToQueue is full, wait end")
        if (isSeek) {
            return false;
        }
    }
    LOGD("pushFrameToQueue pts:%ld(%f)", frame->pts, frame->pts * av_q2d(frame->time_base))
    queue->push(frame);
    suc = true;
    return suc;
}

void FFMpegPlayer::updatePlayerState(PlayerState state) {
    if (mPlayerState != state) {
        LOGI("updatePlayerState from %d to %d", mPlayerState, state);
        mPlayerState = state;
    }
}

void FFMpegPlayer::onPlayCompleted(JNIEnv *env) {
    if (mPlayerJni.isValid()) {
        env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onPlayCompleted);
    }
}