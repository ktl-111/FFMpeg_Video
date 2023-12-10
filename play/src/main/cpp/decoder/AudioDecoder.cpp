#include "AudioDecoder.h"
#include "../utils/loghelper.h"
#include "../utils/CommonUtils.h"

AudioDecoder::AudioDecoder(int index, AVFormatContext *ftx) : BaseDecoder(index, ftx) {
    LOGI("AudioDecoder")
}

AudioDecoder::~AudioDecoder() {
    LOGI("~AudioDecoder")
    release();
}

int sampleConvertOpensles(int sample_rate);

bool AudioDecoder::prepare(JNIEnv *env) {
    AVCodecParameters *params = mFtx->streams[getStreamIndex()]->codecpar;

    mAudioCodec = avcodec_find_decoder(params->codec_id);
    if (mAudioCodec == nullptr) {
        std::string msg = "[audio] not find audio decoder";
        if (mErrorMsgListener) {
            mErrorMsgListener(-1000, msg);
        }
        return false;
    }

    // init codec context
    mCodecContext = avcodec_alloc_context3(mAudioCodec);
    if (!mCodecContext) {
        std::string msg = "[audio] codec context alloc failed";
        if (mErrorMsgListener) {
            mErrorMsgListener(-2000, msg);
        }
        return false;
    }
    avcodec_parameters_to_context(mCodecContext, params);

    // open codec
    mCodecContext->flags2 |= AV_CODEC_FLAG2_SKIP_MANUAL;
    int ret = avcodec_open2(mCodecContext, mAudioCodec, nullptr);
    if (ret != 0) {
        std::string msg = "[audio] codec open failed";
        if (mErrorMsgListener) {
            mErrorMsgListener(-3000, msg);
        }
        return false;
    }


    /**
    * opensl es 使用三步骤
    * 1.创建 create
    * 2.实例化 realize
    * 3.使用对应接口 GetInterface(xxx)
    * */
    SLObjectItf engine;
    //获取引擎
    auto result = slCreateEngine(&engine, 0, 0, 0, 0, 0);

    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio init fail %d", result);
        return false;
    }
    SLEngineItf engineObject;
    //实例化引擎,false:同步,true:异步
    result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio engine Realize fail %d", result);
        return false;
    }
    //执行获取引擎对象接口
    result = (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineObject);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_ENGINE fail %d", result);
        return false;
    }

    //创建多路混音器
    SLObjectItf mix;
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineObject)->CreateOutputMix(engineObject, &mix, 1, mids, mreq);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio CreateOutputMix fail %d", result);
        return false;
    }
    //实例化混音器
    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio mix Realize failed %d", result);
        return false;
    }

    //配置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSnk = {&outputMix, nullptr};

    //创建pcm 配置
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,//pcm格式
            static_cast<SLuint32>(mCodecContext->channels),//声道
            static_cast<SLuint32>(SL_SAMPLINGRATE_44_1),//采样率
            SL_PCMSAMPLEFORMAT_FIXED_16,//bits
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    //定位要播放声音数据的存放位置
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataSource audioSrc = {&que, &pcm};

    //创建播放器
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE};
    SLObjectItf audioPlayObject;
    result = (*engineObject)->CreateAudioPlayer(engineObject, &audioPlayObject, &audioSrc,
                                                &audioSnk, 1,
                                                ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio CreateAudioPlayer failed %d", result);
        return false;
    }

    //实例化
    result = (*audioPlayObject)->Realize(audioPlayObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio audioPlayObject Realize failed %d", result);
        return false;
    }

    //获取操作对象
    SLPlayItf operateObject;
    result = (*audioPlayObject)->GetInterface(audioPlayObject, SL_IID_PLAY, &operateObject);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_PLAY failed %d", result);
        return false;
    }

    //使用接口,获取注册缓冲区回调对象
    result = (*audioPlayObject)->GetInterface(audioPlayObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_BUFFERQUEUE fail %d", result);
        return false;
    }
    //注册回调
//    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, 0, 0);

    //设置播放
    (*operateObject)->SetPlayState(operateObject, SL_PLAYSTATE_PLAYING);

    LOGI("initAudio done");

    mAvFrame = av_frame_alloc();

    mSwrContext = swr_alloc_set_opts(
            nullptr,
            AV_CH_LAYOUT_STEREO,
            AV_SAMPLE_FMT_S16,
            44100,

            (int64_t) mCodecContext->channel_layout,
            mCodecContext->sample_fmt,
            mCodecContext->sample_rate,
            0,
            nullptr
    );
    ret = swr_init(mSwrContext);

    LOGI(
            "[audio] prepare, sample rate: %d, channels: %d, channel_layout: %" PRId64 ", fmt: %d, swr_init: %d",
            mCodecContext->sample_rate, mCodecContext->channels, mCodecContext->channel_layout,
            mCodecContext->sample_fmt, ret);

    mStartTimeMsForSync = -1;

    return ret == 0;
}

int AudioDecoder::decode(AVPacket *avPacket,AVFrame *frame) {
    int64_t start = getCurrentTimeMs();
    int sendRes = avcodec_send_packet(mCodecContext, avPacket);
    int index = av_index_search_timestamp(mFtx->streams[getStreamIndex()], avPacket->pts,
                                          AVSEEK_FLAG_BACKWARD);
    int64_t sendPoint = getCurrentTimeMs() - start;
    LOGI("[audio] avcodec_send_packet...pts: %" PRId64 ", res: %d, index: %d", avPacket->pts,
         sendRes, index)

    // avcodec_send_packet的-11表示要先读output，然后pkt需要重发
    mNeedResent = sendRes == AVERROR(EAGAIN);

    int receiveRes = AVERROR_EOF;
    int receiveCount = 0;
    do {
        start = getCurrentTimeMs();
        // avcodec_receive_frame的-11，表示需要发新帧
        receiveRes = avcodec_receive_frame(mCodecContext, mAvFrame);
        if (receiveRes != 0) {
            LOGE("[audio] avcodec_receive_frame err: %d, resent: %d", receiveRes, mNeedResent)
            av_frame_unref(mAvFrame);
            break;
        }

        int64_t receivePoint = getCurrentTimeMs() - start;
        auto ptsMs = mAvFrame->pts * av_q2d(mFtx->streams[getStreamIndex()]->time_base) * 1000;
        LOGI("[audio] avcodec_receive_frame...pts: %" PRId64 ", time: %f, need retry: %d",
             mAvFrame->pts, ptsMs, mNeedResent)

        int nb = resample(mAvFrame);

        updateTimestamp(mAvFrame);

        int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

        if (nb > 0) {
            mDataSize = nb * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
            if (mOnFrameArrivedListener != nullptr) {
                mOnFrameArrivedListener(mAvFrame);
            }
        }
        LOGI("swr_convert, dataSize: %d, nb: %d, out_channels: %d", mDataSize, nb,
             out_channels)
        mDataSize = 0;
        mNeedFlushRender = false;

        av_frame_unref(mAvFrame);
        receiveCount++;

        LOGW(
                "[audio] decode sendPoint: %" PRId64 ", receivePoint: %" PRId64 ", receiveCount: %d",
                sendPoint, receivePoint, receiveCount)
    } while (true);
    return receiveRes;
}

void AudioDecoder::playAudio(AVFrame *frame) {
    LOGI("audio pcmBufferCallBack start");
    if (mDataSize > 0) {
        LOGI("audio pcmBufferQueue Enqueue");
        (*pcmBufferQueue)->Enqueue(pcmBufferQueue, mAudioBuffer, mDataSize);
    }
}

int AudioDecoder::resample(AVFrame *frame) {
    int out_nb = (int) av_rescale_rnd(frame->nb_samples, 44100, frame->sample_rate, AV_ROUND_UP);
    int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    int size = av_samples_get_buffer_size(nullptr, out_channels, out_nb, AV_SAMPLE_FMT_S16, 1);
    if (mAudioBuffer == nullptr) {
        LOGI("audio frame, out_channels: %d, out_nb: %d, size: %d, ", out_channels, out_nb,
             size)
        mAudioBuffer = (uint8_t *) av_malloc(size);
    }

    int nb = swr_convert(
            mSwrContext,
            &mAudioBuffer,
            size / out_channels,
            (const uint8_t **) frame->data,
            frame->nb_samples
    );
    return nb;
}

void AudioDecoder::updateTimestamp(AVFrame *frame) {
    if (mStartTimeMsForSync < 0) {
        LOGE("update audio start time")
        mStartTimeMsForSync = getCurrentTimeMs();
    }

    // s -> ms
    mCurTimeStampMs = (int64_t) (frame->pts * av_q2d(mTimeBase) * 1000);

    if (mFixStartTime) {
        mStartTimeMsForSync = getCurrentTimeMs() - mCurTimeStampMs;
        mFixStartTime = false;
        LOGE("fix audio start time")
    }
}

int64_t AudioDecoder::getTimestamp() const {
    return mCurTimeStampMs;
}

void AudioDecoder::avSync(AVFrame *frame) {
    int64_t elapsedTimeMs = getCurrentTimeMs() - mStartTimeMsForSync;
    int64_t diff = mCurTimeStampMs - elapsedTimeMs;
    diff = FFMIN(diff, DELAY_THRESHOLD);
    LOGI("[audio] avSync, pts: %" PRId64 "ms, diff: %" PRId64 "ms", mCurTimeStampMs, diff)
    if (diff > 0) {
        av_usleep(diff * 1000);
    } else {
        LOGE("avSync warning")
    }
}

double AudioDecoder::getDuration() {
    return mDuration;
}

int AudioDecoder::seek(double pos) {
//    flush();
//    int64_t seekPos = av_rescale_q((int64_t) (pos * AV_TIME_BASE), AV_TIME_BASE_Q, mTimeBase);
//    int ret = avformat_seek_file(mFtx, getStreamIndex(),
//                                 INT64_MIN, seekPos, INT64_MAX,
//                                 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
//    LOGE("[audio] seek to: %f, seekPos: %" PRId64 ", ret: %d", pos, seekPos, ret)
    // seek后需要恢复起始时间
    mFixStartTime = true;
    mNeedFlushRender = true;
    return 1;
}

void AudioDecoder::release() {
    mFixStartTime = false;
    mNeedFlushRender = false;
    if (mAudioBuffer != nullptr) {
        av_free(mAudioBuffer);
        mAudioBuffer = nullptr;
        LOGI("[audio] buffer...release")
    }

    if (mAvFrame != nullptr) {
        av_frame_free(&mAvFrame);
        av_freep(&mAvFrame);
        LOGI("[audio] av frame...release")
    }

    if (mSwrContext != nullptr) {
        swr_free(&mSwrContext);
        mSwrContext = nullptr;
        LOGI("[audio] sws context...release")
    }

    if (mCodecContext != nullptr) {
        avcodec_free_context(&mCodecContext);
        mCodecContext = nullptr;
        LOGI("[audio] codec...release")
    }
}

int sampleConvertOpensles(int sample_rate) {
    int rate = 0;
    switch (sample_rate) {
        case 8000:
            rate = SL_SAMPLINGRATE_8;
            break;
        case 11025:
            rate = SL_SAMPLINGRATE_11_025;
            break;
        case 12000:
            rate = SL_SAMPLINGRATE_12;
            break;
        case 16000:
            rate = SL_SAMPLINGRATE_16;
            break;
        case 22050:
            rate = SL_SAMPLINGRATE_22_05;
            break;
        case 24000:
            rate = SL_SAMPLINGRATE_24;
            break;
        case 32000:
            rate = SL_SAMPLINGRATE_32;
            break;
        case 44100:
            rate = SL_SAMPLINGRATE_44_1;
            break;
        case 48000:
            rate = SL_SAMPLINGRATE_48;
            break;
        case 64000:
            rate = SL_SAMPLINGRATE_64;
            break;
        case 88200:
            rate = SL_SAMPLINGRATE_88_2;
            break;
        case 96000:
            rate = SL_SAMPLINGRATE_96;
            break;
        case 192000:
            rate = SL_SAMPLINGRATE_192;
            break;
        default:
            rate = SL_SAMPLINGRATE_44_1;

    }
    return rate;

}

