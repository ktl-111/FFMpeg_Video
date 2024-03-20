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
            //视频流
            LOGI("video stream,index:%d result:%d", i, mPlayerJni.isValid())
            mVideoDecoder = std::make_shared<VideoDecoder>(i, mAvFormatContext);
            std::shared_ptr<OutConfig> outConfig;
            if (out_config) {
                jclass outConfigClass = env->GetObjectClass(out_config);
                jfieldID outWidthId = env->GetFieldID(outConfigClass, "width", "I");
                jint outWidth = env->GetIntField(out_config, outWidthId);

                jfieldID outHeightId = env->GetFieldID(outConfigClass, "height", "I");
                jint outHeight = env->GetIntField(out_config, outHeightId);

                jfieldID cropWidthId = env->GetFieldID(outConfigClass, "cropWidth", "I");
                jint cropWidth = env->GetIntField(out_config, cropWidthId);

                jfieldID cropHeightId = env->GetFieldID(outConfigClass, "cropHeight", "I");
                jint cropHeight = env->GetIntField(out_config, cropHeightId);

                jfieldID outFpsId = env->GetFieldID(outConfigClass, "fps", "D");
                jdouble outFps = env->GetDoubleField(out_config, outFpsId);

                int videoWidth = mVideoDecoder->getWidth();
                int videoHeight = mVideoDecoder->getHeight();
                if (videoHeight > videoWidth) {
                    //竖屏
                    int temp = outWidth;
                    outWidth = outHeight;
                    outHeight = temp;

                    temp = cropWidth;
                    cropWidth = cropHeight;
                    cropHeight = temp;
                }
                if (cropHeight != 0 && cropWidth != 0) {
                    if (videoWidth < cropWidth) {
                        outWidth = cropWidth;
                        outHeight = 0;
                    }
                    if (videoHeight < cropHeight) {
                        outHeight = cropHeight;
                        outWidth = 0;
                    }
                }

                if (outWidth <= 0 && outHeight <= 0) {
                    outWidth = videoWidth;
                    outHeight = videoHeight;
                } else if (outWidth > 0) { // scale base width
                    outWidth += outWidth % 2;
                    outHeight = (jint) (1.0 * outWidth * videoHeight / videoWidth);
                    outHeight += outHeight % 2;
                } else { // scale base height
                    outHeight += outHeight % 2;
                    outWidth = (jint) (1.0 * outHeight * videoWidth / videoHeight);
                    outWidth += outWidth % 2;
                }
                outConfig = std::make_shared<OutConfig>(outWidth, outHeight, cropWidth, cropHeight,
                                                        outFps);

                LOGI("set out config,video:%d*%d,out:%d*%d,crop:%d*%d,fps:%f",
                     videoWidth, videoHeight,
                     outWidth, outHeight, cropWidth, cropHeight, outFps)
            } else {
                LOGE("not out config")
            }

            int frameSize = 50;
            if (outConfig) {
                mVideoDecoder->setOutConfig(outConfig);
//                frameSize = outConfig->getFps() * 5;
            }

            mVideoDecoder->setSurface(surface);
            videoPrepared = mVideoDecoder->prepare(env);
            if (surface != nullptr && !videoPrepared) {
                mVideoDecoder->release();
                LOGE("[video] hw decoder prepare failed, fallback to software decoder")
                mVideoDecoder->setSurface(nullptr);
                videoPrepared = mVideoDecoder->prepare(env);
            }

            if (mPlayerJni.isValid()) {
                int surfaceWidth = mVideoDecoder->getWidth();
                int surfaceHeight = mVideoDecoder->getHeight();
                if (outConfig && outConfig->getCropWidth() != 0 &&
                    outConfig->getCropHeight() != 0) {
                    surfaceWidth = outConfig->getCropWidth();
                    surfaceHeight = outConfig->getCropHeight();
                }
                env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onVideoConfig,
                                    surfaceWidth, surfaceHeight,
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
    mVideoPacketQueue = std::make_shared<AVPacketQueue>(50);
    mVideoFrameQueue = std::make_shared<AVFrameQueue>(50);
    mVideoThread = new std::thread(&FFMpegPlayer::VideoDecodeLoop, this);
    mReadPacketThread = new std::thread(&FFMpegPlayer::ReadPacketLoop, this);
    mVideoDecodeThread = new std::thread(&FFMpegPlayer::ReadVideoFrameLoop, this);
    return prepared;
}

void FFMpegPlayer::start() {
    LOGI("FFMpegPlayer::start, state: %d", mPlayerState)
    resume();
}

bool FFMpegPlayer::seekTo(int64_t seekTime) {
    pause();
    LOGI("seek start:%ld", seekTime)
    int64_t d = (int64_t) mVideoDecoder->getDuration() * 1000;
    if (seekTime > d) {
        LOGI("seek seekTime(%ld)>dration(%ld)", seekTime, d)
        return false;
    }
    int64_t currPlayTime = mVideoDecoder->getTimestamp();
    if (currPlayTime == seekTime) {
        LOGI("seek currPlayTime(%ld) == seekTime(%ld)", currPlayTime, seekTime)
        return true;
    }
    mVideoDecoder->seekLock();
    mCurrSeekTime = seekTime;
    mIsSeek = true;

    AVFrame *pFrame;
    int64_t diffFps = 1000.0f / mVideoDecoder->getFps();
    int64_t lastCacheFrameTime =
            currPlayTime + (mVideoFrameQueue->isEmpty() ? 0 : mVideoFrameQueue->getSize()) *
                           diffFps;
    mIsBackSeek = currPlayTime > seekTime;
    LOGI("seek seekTime:%ld,currPlayTime:%ld,lastCache:%ld", seekTime, currPlayTime,
         lastCacheFrameTime)
    bool callSeek = false;
    bool callQueueClear = false;
    if (seekTime > lastCacheFrameTime || mIsBackSeek) {
        callSeek = true;
        callQueueClear = true;
        if (mIsBackSeek) {
            pFrame = av_frame_alloc();
            if (mVideoFrameQueue->front(pFrame) == 0) {
                int64_t diff = pFrame->pts * av_q2d(pFrame->time_base) * 1000 - seekTime;
                LOGI("seek front frame %ld(%lf) diff:%ld diffFps:%ld", pFrame->pts,
                     pFrame->pts * av_q2d(pFrame->time_base) * 1000, diff, diffFps)
                if (diff < diffFps * 2) {
                    callSeek = false;
                    callQueueClear = false;
                }
            }
            av_frame_free(&pFrame);
        }
        LOGI("seek callSeek:%d callQueueClear:%d", callSeek, callQueueClear)
        if (callSeek) {
            mVideoDecoder->seek(seekTime);
        }
        if (callQueueClear) {
            mVideoPacketQueue->clear();
            mVideoFrameQueue->clear();
        }
    }
    mVideoDecoder->seekUnlock();
    if (callSeek) {
        LOGI("seek back wait start")
        mVideoFrameQueue->wait();
        LOGI("seek back wait end")
        if (mHasAbort) {
            return false;
        }
    }
    pFrame = av_frame_alloc();

    int ret = mVideoFrameQueue->getFrameByTime(pFrame, seekTime, mIsBackSeek);
    if (ret != 0) {
        LOGE("seek %ld get frame fail %d", seekTime, ret)
        av_frame_free(&pFrame);
        pFrame = nullptr;
//        if (ret == AVERROR(EAGAIN)) {
//            LOGI("seek %lf start", seekTime)
//            mVideoFrameQueue->wait();
//            LOGI("seek %lf end", seekTime)
//            return seekTo(seekTime);
//        }
        return false;
    }
    int64_t frameTime = pFrame->pts * av_q2d(pFrame->time_base) * 1000;
//    if (!mIsBackSeek && seekTime > frameTime) {
//        LOGI("seek seekTime:%ld>frameTime %ld,continue seek", seekTime, frameTime)
//        av_frame_free(&pFrame);
//        return seekTo(seekTime);
//    }
    LOGI("seek %ld done,show %ld", seekTime, frameTime)
    mVideoDecoder->resultCallback(pFrame);
    av_frame_free(&pFrame);
    LOGI("seek %ld done", seekTime)
    return true;
}

void FFMpegPlayer::stop() {
    LOGI("FFMpegPlayer::stop")
    mHasAbort = true;
    mIsMute = false;
    updatePlayerState(PlayerState::STOP);
    if (mVideoPacketQueue) {
        mVideoPacketQueue->notify();
    }
    if (mVideoFrameQueue) {
        mVideoFrameQueue->notify();
    }
    mMutexObj->wakeUp();

    if (mReadPacketThread != nullptr) {
        LOGE("join mReadPacketThread")
        mReadPacketThread->join();
        delete mReadPacketThread;
        mReadPacketThread = nullptr;
        LOGE("release mReadPacketThread")
    }
    if (mVideoDecodeThread != nullptr) {
        LOGE("join mVideoDecodeThread")
        mVideoDecodeThread->join();
        delete mVideoDecodeThread;
        mVideoDecodeThread = nullptr;
        LOGE("release mVideoDecodeThread")
    }

    if (mVideoThread != nullptr) {
        LOGE("join mVideoThread")
        mVideoThread->join();
        delete mVideoThread;
        mVideoThread = nullptr;
        LOGE("release mVideoThread")
    }
    mVideoDecoder = nullptr;
    LOGE("release video res")

    mVideoFrameQueue->clear();
    mVideoFrameQueue = nullptr;

    mVideoPacketQueue->clear();
    mVideoPacketQueue = nullptr;

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
    mIsSeek = false;
    mIsBackSeek = false;
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
    JNIEnv *env = nullptr;
    bool needAttach = mJvm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_EDETACHED;
    if (needAttach) {
        mJvm->AttachCurrentThread(&env, nullptr);
        LOGE("[video] AttachCurrentThread")
    }
    mVideoDecoder->setOnFrameArrived([this](AVFrame *frame) {
        JNIEnv *env = nullptr;
        bool needAttach = mJvm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_EDETACHED;
        if (needAttach) {
            mJvm->AttachCurrentThread(&env, nullptr);
            LOGE("[video] OnFrameArrived AttachCurrentThread")
        }
        LOGI("OnFrameArrived start needAttach:%d", needAttach)
        if (!mHasAbort && mVideoDecoder) {
            if (mAudioDecoder) {
                auto diff = mAudioDecoder->getTimestamp() - mVideoDecoder->getTimestamp();
                LOGW("[video] frame arrived, AV time diff: %ld,mIsSeek: %d", diff, mIsSeek)
            }
            if (!mIsSeek) {
                int64_t timestamp = mVideoDecoder->getTimestamp();
                LOGI("avSync start %ld,mIsSeek: %d", timestamp, mIsSeek)
                mVideoDecoder->avSync(frame);
                LOGI("avSync end %ld,mIsSeek: %d", timestamp, mIsSeek)
                if (mIsSeek) {
                    return;
                }
            }
            mVideoDecoder->showFrameToWindow(frame);
            LOGI("async done")
            if (!mIsSeek && !mAudioDecoder && mPlayerJni.isValid()) { // no audio track
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

    LOGI("[video] VideoDecodeLoop start")
    while (true) {
        if (!mHasAbort) {
            mVideoFrameQueue->checkEmptyWait();
        }

        if (mPlayerState == PlayerState::PAUSE || mPlayerState == PlayerState::PREPARE) {
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
                if (frame->pkt_size == 0) {
                    onPlayCompleted(env);
                    LOGI("[video] VideoDecodeLoop AVERROR_EOF wait start")
                    av_frame_free(&frame);
                    mVideoFrameQueue->wait();
                    LOGI("[video] VideoDecodeLoop AVERROR_EOF wait end")
                } else {
                    LOGI("[video] VideoDecodeLoop pts:%ld(%lf)", frame->pts,
                         frame->pts * av_q2d(frame->time_base))
                    mVideoDecoder->resultCallback(frame);
                    av_frame_free(&frame);
                }
            } else {
                LOGE("VideoDecodeLoop pop frame failed...")
            }
        }
    }
    if (needAttach) {
        mJvm->DetachCurrentThread();
    }
    LOGI("[video] VideoDecodeLoop end")
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
    LOGI("ReadVideoFrameLoop start")
    while (true) {
        if (!mHasAbort) {
            mVideoPacketQueue->checkEmptyWait();
        }

        if (mHasAbort) {
            LOGE("[video] ReadVideoFrameLoop has abort...")
            break;
        }

        int decodeResult;

        do {
            mVideoDecoder->seekLock();
            decodeResult = -1;
            AVPacket *packet = av_packet_alloc();
            int ret = mVideoPacketQueue->popTo(packet);
            std::shared_ptr<AVFrameQueue> tempFrameQueue = std::make_shared<AVFrameQueue>(50);
            if (ret == 0) {
                LOGI("ReadVideoFrameLoop popto pts:%ld ret:%d size:%ld", packet->pts, ret,
                     mVideoFrameQueue->getSize())
                do {
                    AVFrame *pFrame = av_frame_alloc();
                    decodeResult = mVideoDecoder->decode(packet, pFrame);
                    if (mHasAbort) {
                        decodeResult = -1;
                        break;
                    }
                    if (decodeResult == 0) {
                        tempFrameQueue->push(pFrame);
                    }
                } while (mVideoDecoder->isNeedResent());
                LOGI("[video] ReadVideoFrameLoop decode %d,tempQueueSize:%ld", decodeResult,
                     tempFrameQueue->getSize())
            } else {
                LOGE("ReadVideoFrameLoop pop packet failed...")
            }
            av_packet_free(&packet);
            mVideoDecoder->seekUnlock();
            while (!tempFrameQueue->isEmpty()) {
                AVFrame *pFrame = av_frame_alloc();
                if (tempFrameQueue->popTo(pFrame) == 0) {
                    if (!pushFrameToQueue(pFrame, mVideoFrameQueue)) {
                        av_frame_free(&pFrame);
                    } else {
                        LOGI("pushFrameToQueue success %ld", pFrame->pts)
                    }
                } else {
                    av_frame_free(&pFrame);
                }
            }
            tempFrameQueue = nullptr;
            if (decodeResult == AVERROR_EOF) {
                AVFrame *doneFrame = av_frame_alloc();
                doneFrame->pkt_size = 0;
                pushFrameToQueue(doneFrame, mVideoFrameQueue);
                LOGI("ReadVideoFrameLoop AVERROR_EOF")
                break;
            }
            if (mHasAbort) {
                break;
            }
        } while (decodeResult == AVERROR(EAGAIN));
    }

    LOGI("ReadVideoFrameLoop end")
}

void FFMpegPlayer::ReadPacketLoop() {
    LOGI("FFMpegPlayer::ReadPacketLoop start")
    while (mPlayerState != PlayerState::STOP) {
        if (mHasAbort) {
            break;
        }
        bool isEnd = readAvPacketToQueue(ReadPackType::ANY) != 0;
        if (isEnd) {
            LOGW("read av packet end,wait start, mPlayerState: %d", mPlayerState)
            mVideoPacketQueue->wait();
            LOGW("read av packet end,wait end, mPlayerState: %d", mPlayerState)
        }
    }
    LOGI("FFMpegPlayer::ReadPacketLoop end")
}

bool FFMpegPlayer::readAvPacketToQueue(ReadPackType type) {
    reread:
    AVPacket *avPacket = av_packet_alloc();
    mVideoDecoder->seekLock();
    bool isEnd;
    int ret = av_read_frame(mAvFormatContext, avPacket);
    mVideoDecoder->seekUnlock();
    bool suc = false;
    if (ret == 0) {
        if (type == ReadPackType::VIDEO) {
            if (avPacket->stream_index != mVideoDecoder->getStreamIndex()) {
                av_packet_free(&avPacket);
                goto reread;
            }
        } else if (type == ReadPackType::AUDIO) {
            if (avPacket->stream_index != mAudioDecoder->getStreamIndex()) {
                av_packet_free(&avPacket);
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
        isEnd = false;
    } else {
        // send flush packet
        AVPacket *videoFlushPkt = av_packet_alloc();
        videoFlushPkt->size = 0;
        videoFlushPkt->data = nullptr;
        isEnd = true;
        if (!pushPacketToQueue(videoFlushPkt, mVideoPacketQueue)) {
            av_packet_free(&videoFlushPkt);
            av_freep(&videoFlushPkt);
            isEnd = false;
        }

        if (mAudioPacketQueue) {
            AVPacket *audioFlushPkt = av_packet_alloc();
            audioFlushPkt->size = 0;
            audioFlushPkt->data = nullptr;
            if (!pushPacketToQueue(audioFlushPkt, mAudioPacketQueue)) {
                av_packet_free(&audioFlushPkt);
                av_freep(&audioFlushPkt);
            }
        }
        LOGE("read packet...end or failed: %d", ret)

    }

    if (!suc) {
        LOGE("av_read_frame, other...pts: %" PRId64 ", index: %d", avPacket->pts,
             avPacket->stream_index)
        av_packet_free(&avPacket);
    }
    return isEnd;
}


bool FFMpegPlayer::pushPacketToQueue(AVPacket *packet,
                                     const std::shared_ptr<AVPacketQueue> &queue) const {
    if (queue == nullptr) {
        return false;
    }

    bool suc = false;
    while (queue->isFull()) {
        LOGI("pushPacketToQueue is full, wait start")
        queue->wait();
        LOGI("pushPacketToQueue is full, wait end")
        if (mHasAbort) {
            return false;
        }
        if (mIsSeek && queue->isEmpty()) {
            LOGD("pushPacketToQueue is full, wait end and filter")
            return false;
        }
    }
    LOGI("pushPacketToQueue pts:%ld size:%d", packet->pts, packet->size)
    if (packet->size == 0 && packet->data == nullptr) {
        if (queue->checkLastIsEofPack()) {
            LOGI("pushPacketToQueue last is eof.filter curr packet")
            return true;
        }
    }
    queue->push(packet);
    suc = true;
    return suc;
}

bool FFMpegPlayer::pushFrameToQueue(AVFrame *frame,
                                    const std::shared_ptr<AVFrameQueue> &queue) {
    if (queue == nullptr) {
        return false;
    }

    bool suc = false;
    while (queue->isFull()) {
        LOGD("pushFrameToQueue is full, wait start")
        queue->wait();
        LOGD("pushFrameToQueue is full, wait end")
        if (mHasAbort) {
            return false;
        }
        if (mIsSeek && queue->isEmpty()) {
            LOGD("pushFrameToQueue is full, wait end and filter")
            return false;
        }

    }
    LOGD("pushFrameToQueue pts:%ld(%f),pkt_size:(%d),mCurrSeekTime:%ld,mIsBackSeek:%d", frame->pts,
         frame->pts * av_q2d(frame->time_base), frame->pkt_size, mCurrSeekTime, mIsBackSeek)
    if (mIsSeek) {
        if (frame->pkt_size == 0) {
            LOGI("pushFrameToQueue is seek,filter eof frame")
            return false;
        }
        int64_t diff = (int64_t) (frame->pts * av_q2d(frame->time_base) * 1000) - mCurrSeekTime;
        bool push;
        if (mIsBackSeek) {
            if (queue->isEmpty()) {
                int64_t diffFps = 1000.0f / mVideoDecoder->getFps();
                LOGI("pushFrameToQueue check back seek diff:%ld diffFps:%ld", diff, diffFps)
                if (diff >= 0) {
                    push = diff <= diffFps * 2;
                } else {
                    push = false;
                }
            } else {
                push = true;
            }
        } else {
            push = diff >= 0;
        }
        if (push) {
            queue->push(frame);
        } else {
            return false;
        }
    } else {
        if (frame->pkt_size == 0) {
            if (queue->checkLastIsEofFrame()) {
                LOGI("pushFrameToQueue last is eof.filter curr frame")
                return false;
            }
        }
        queue->push(frame);
        if (mShowFirstFrame) {
            mVideoDecoder->resultCallback(frame);
            mShowFirstFrame = false;
        }
    }
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
    updatePlayerState(PlayerState::PAUSE);
    if (mPlayerJni.isValid()) {
        env->CallVoidMethod(mPlayerJni.instance, mPlayerJni.onPlayCompleted);
    }
}

void FFMpegPlayer::surfaceReCreate(JNIEnv *env, jobject surface) {
    mVideoDecoder->surfaceReCreate(env, surface);
}

void FFMpegPlayer::surfaceDestroy(JNIEnv *env) {
    mVideoDecoder->surfaceDestroy(env);
}

int64_t FFMpegPlayer::getCurrTimestamp() {
    if (mVideoDecoder) {
        return mVideoDecoder->getTimestamp();
    }
    return 0;
}

int FFMpegPlayer::getPlayerState() {
    return mPlayerState;
}

