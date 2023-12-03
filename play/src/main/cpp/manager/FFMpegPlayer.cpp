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

bool FFMpegPlayer::prepare(JNIEnv *env, std::string &path, jobject surface) {
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
            //视频流
            LOGI("video stream,index:%d result:%d", i, mPlayerJni.isValid())
            mVideoDecoder = std::make_shared<VideoDecoder>(i, mAvFormatContext);
            mVideoPacketQueue = std::make_shared<AVPacketQueue>(50);
            mVideoThread = new std::thread(&FFMpegPlayer::VideoDecodeLoop, this);

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
            if (audioPrePared) {
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

bool isSeek = false;

bool FFMpegPlayer::seekTo(double seekTime) {
    pause();
    isSeek = true;
    if (mVideoDecoder) {
        mVideoDecoder->seek(seekTime);
        mVideoPacketQueue->clear();
    }
    if (mAudioDecoder) {
        mAudioDecoder->seek(seekTime);
        mAudioPacketQueue->clear();
    }
    LOGI("seekTo %lf start read packet", seekTime)
    int result = readAvPacketToQueue(ReadPackType::VIDEO);
    if (result != 0) {
        LOGI("seekTo %lf fail", seekTime)
        return false;
    }
    AVPacket *packet = av_packet_alloc();
    if (packet != nullptr) {
        int ret = mVideoPacketQueue->popTo(packet);
        if (ret == 0) {
            do {
                ret = mVideoDecoder->decode(packet);
            } while (mVideoDecoder->isNeedResent());

            av_packet_unref(packet);
            av_packet_free(&packet);
            if (ret == AVERROR_EOF) {
                LOGE("VideoDecodeLoop AVERROR_EOF")
            }
        } else {
            LOGE("VideoDecodeLoop pop packet failed...")
        }
    }

    LOGI("seekTo %lf done", seekTime)
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

    mHasAbort = true;
    mIsMute = false;
    mIsSeek = false;
    mVideoSeekPos = -1;
    mAudioSeekPos = -1;

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
    mMutexObj->wakeUp();
}

void FFMpegPlayer::pause() {
    updatePlayerState(PlayerState::PAUSE);
}


void FFMpegPlayer::VideoDecodeLoop() {
    if (mVideoDecoder == nullptr || mVideoPacketQueue == nullptr) {
        return;
    }
    JNIEnv *env = nullptr;
    if (mJvm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_EDETACHED) {
        mJvm->AttachCurrentThread(&env, nullptr);
        LOGE("[video] AttachCurrentThread")
    }

    mVideoDecoder->setOnFrameArrived([this, env](AVFrame *frame) {
        if (!mHasAbort && mVideoDecoder) {
            if (mAudioDecoder) {
                auto diff = mAudioDecoder->getTimestamp() - mVideoDecoder->getTimestamp();
                LOGW("[video] frame arrived, AV time diff: %ld,isSeek: %d", diff, isSeek)
            }
            if (!isSeek) {
                int64_t timestamp = mVideoDecoder->getTimestamp();
                LOGI("avSync start %ld,isSeek: %d", timestamp,isSeek)
                mVideoDecoder->avSync(frame);
                LOGI("avSync end %ld,isSeek: %d", timestamp,isSeek)
                if (isSeek) {
                    return;
                }
            }
            mVideoDecoder->showFrameToWindow(frame);
            if (!mAudioDecoder && mPlayerJni.isValid()) { // no audio track
                double timestamp = mVideoDecoder->getTimestamp();
                env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onPlayProgress, timestamp);
            }
        } else {
            LOGE("[video] setOnFrameArrived, has abort")
        }
    });

    while (true) {
        if (mVideoSeekPos >= 0) {
            mVideoPacketQueue->clear();
            mVideoDecoder->seek(mVideoSeekPos);
            mVideoSeekPos = -1;
            LOGI("clear video queue via seek")
            mMutexObj->wakeUp();
        }

        while (!mHasAbort && mVideoPacketQueue->isEmpty() && mVideoSeekPos < 0) {
            LOGE("[video] no packet, wait...")
            mVideoPacketQueue->wait();
        }

        if (mPlayerState == PlayerState::PAUSE) {
            LOGI("decode pause wait")
            mMutexObj->wait();
            LOGI("decode pause wakup state:%d", mPlayerState);
        }

        if (mHasAbort) {
            LOGE("[video] has abort...")
            break;
        }

        AVPacket *packet = av_packet_alloc();
        if (packet != nullptr) {
            int ret = mVideoPacketQueue->popTo(packet);
            if (ret == 0) {
                do {
                    ret = mVideoDecoder->decode(packet);
                } while (mVideoDecoder->isNeedResent());

                av_packet_unref(packet);
                av_packet_free(&packet);
                if (ret == AVERROR_EOF) {
                    LOGE("VideoDecodeLoop AVERROR_EOF")
                    if (!mAudioDecoder) { // 存在音轨以音频播放结束为准
//                        onPlayCompleted(env);
                    }
                }
            } else {
                LOGE("VideoDecodeLoop pop packet failed...")
            }
        }
    }

    mVideoPacketQueue->clear();
    mVideoPacketQueue = nullptr;

    mJvm->DetachCurrentThread();
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
        if (mAudioSeekPos >= 0) {
            mAudioPacketQueue->clear();
            mAudioDecoder->seek(mAudioSeekPos);
            mAudioSeekPos = -1;
            LOGI("clear audio queue via seek")
            mMutexObj->wakeUp();
        }

        while (!mHasAbort && mAudioPacketQueue->isEmpty() && mAudioSeekPos < 0) {
            LOGE("[audio] no packet, wait...")
            mAudioPacketQueue->wait();
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
                    ret = mAudioDecoder->decode(packet);
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

void FFMpegPlayer::ReadPacketLoop() {
    LOGI("FFMpegPlayer::ReadPacketLoop start")
    while (mPlayerState != PlayerState::STOP) {
        if (mPlayerState == PlayerState::PAUSE) {
            LOGW("FFMpegPlayer::ReadPacketLoop wait")
            mMutexObj->wait();
            LOGW("FFMpegPlayer::ReadPacketLoop wakeup")
            // double check stop state
            // or check pause state for pause & seek

            // check mIsSeek: play complete -> seek -> play
            if (mIsSeek && mVideoSeekPos < 0 && mAudioSeekPos < 0) {
                mIsSeek = false;
                LOGW("FFMpegPlayer::ReadPacketLoop, reset seek status")
            }
            continue;
        }

        // check is seek
        bool isSeeking = false;
        while (mVideoSeekPos >= 0 || mAudioSeekPos >= 0) {
            isSeeking = true;
            LOGI("seek wait...mVideoSeekPos: %f, mAudioSeekPos: %f", mVideoSeekPos,
                 mAudioSeekPos)
            mMutexObj->wait();
        }
        if (isSeeking) {
            mIsSeek = false;
            LOGW("FFMpegPlayer::ReadPacketLoop, seek prepare has ready")
        }

        // read packet to queue
        updatePlayerState(PlayerState::PLAYING);
        bool isEnd = readAvPacketToQueue(ReadPackType::ANY) != 0;
        if (isEnd) {
            LOGW("read av packet end, mPlayerState: %d", mPlayerState)
            if (mPlayerState == PlayerState::PLAYING) {
                updatePlayerState(PlayerState::PAUSE);
            }
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
            LOGI("push video ReadPackType:%d", type)
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
        queue->wait(10);
        LOGD("queue is full, wait 10ms, packet index: %d", packet->stream_index)
    }
    if (!mIsSeek) {
        queue->push(packet);
        suc = true;
    } else {
        LOGE("discard packet for seek, packet index: %d", packet->stream_index)
    }
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