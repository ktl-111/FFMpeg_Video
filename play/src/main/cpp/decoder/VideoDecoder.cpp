#include "VideoDecoder.h"
#include "../utils/loghelper.h"
#include "../utils/CommonUtils.h"
#include "../reader/FFVideoReader.h"

static enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt) {
            LOGE("get HW surface format: %d", *p);
            return *p;
        }
    }

    LOGE("Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

VideoDecoder::VideoDecoder(int index, AVFormatContext *ftx) : BaseDecoder(index, ftx) {}

VideoDecoder::~VideoDecoder() {
    release();
}

void VideoDecoder::setSurface(jobject surface) {
    mSurface = surface;
}

bool VideoDecoder::prepare(JNIEnv *env) {
    AVStream *stream = mFtx->streams[getStreamIndex()];

    AVCodecParameters *params = stream->codecpar;
    mWidth = params->width;
    mHeight = params->height;
    mFps = av_q2d(stream->avg_frame_rate);

    // find decoder
    bool useHwDecoder = mSurface != nullptr;
    std::string mediacodecName;
    switch (params->codec_id) {
        case AV_CODEC_ID_H264:
            mediacodecName = "h264_mediacodec";
            break;
        case AV_CODEC_ID_HEVC:
            mediacodecName = "hevc_mediacodec";
            break;
        default:
            useHwDecoder = false;
            LOGE("format(%d) not support hw decode, maybe rebuild ffmpeg so", params->codec_id)
            break;
    }

    if (useHwDecoder) {
        AVHWDeviceType type = av_hwdevice_find_type_by_name("mediacodec");
        if (type == AV_HWDEVICE_TYPE_NONE) {
            while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
                LOGI("av_hwdevice_iterate_types: %d", type)
            }
        }

        const AVCodec *mediacodec = avcodec_find_decoder_by_name(mediacodecName.c_str());
        if (mediacodec) {
            LOGE("find %s", mediacodecName.c_str())
            for (int i = 0;; ++i) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(mediacodec, i);
                if (!config) {
                    LOGE("Decoder: %s does not support device type: %s", mediacodec->name,
                         av_hwdevice_get_type_name(type))
                    break;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == type) {
                    // AV_PIX_FMT_MEDIACODEC(165)
                    hw_pix_fmt = config->pix_fmt;
                    LOGE(
                            "Decoder: %s support device type: %s, hw_pix_fmt: %d, AV_PIX_FMT_MEDIACODEC: %d",
                            mediacodec->name,
                            av_hwdevice_get_type_name(type), hw_pix_fmt, AV_PIX_FMT_MEDIACODEC);
                    break;
                }
            }

            if (hw_pix_fmt == AV_PIX_FMT_NONE) {
                LOGE("not use surface decoding")
                mVideoCodec = avcodec_find_decoder(params->codec_id);
            } else {
                mVideoCodec = mediacodec;
                int ret = av_hwdevice_ctx_create(&mHwDeviceCtx, type, nullptr, nullptr, 0);
                if (ret != 0) {
                    LOGE("av_hwdevice_ctx_create err: %d", ret)
                }
            }
        } else {
            LOGE("not find %s", mediacodecName.c_str())
            mVideoCodec = avcodec_find_decoder(params->codec_id);
        }
    } else {
        mVideoCodec = avcodec_find_decoder(params->codec_id);
    }

    if (mVideoCodec == nullptr) {
        std::string msg = "not find decoder";
        if (mErrorMsgListener) {
            mErrorMsgListener(-1000, msg);
        }
        return false;
    }

    // init codec context
    mCodecContext = avcodec_alloc_context3(mVideoCodec);
    if (!mCodecContext) {
        std::string msg = "codec context alloc failed";
        if (mErrorMsgListener) {
            mErrorMsgListener(-2000, msg);
        }
        return false;
    }
    avcodec_parameters_to_context(mCodecContext, params);

    if (mHwDeviceCtx) {
        mCodecContext->get_format = get_hw_format;
        mCodecContext->hw_device_ctx = av_buffer_ref(mHwDeviceCtx);

        if (mSurface != nullptr) {
            mMediaCodecContext = av_mediacodec_alloc_context();
            av_mediacodec_default_init(mCodecContext, mMediaCodecContext, mSurface);
        }
    } else {
        //创建nativewindow
        nativeWindow = ANativeWindow_fromSurface(env, mSurface);
        //修改缓冲区格式和大小,对应视频格式和大小
        ANativeWindow_setBuffersGeometry(nativeWindow, getWidth(), getHeight(),
                                         WINDOW_FORMAT_RGBA_8888);
    }

    // open codec
    int ret = avcodec_open2(mCodecContext, mVideoCodec, nullptr);
    if (ret != 0) {
        std::string msg = "codec open failed";
        if (mErrorMsgListener) {
            mErrorMsgListener(-3000, msg);
        }
        return false;
    }

    mAvFrame = av_frame_alloc();
    mStartTimeMsForSync = -1;
    mRetryReceiveCount = RETRY_RECEIVE_COUNT;
    LOGI("codec name: %s", mVideoCodec->name)

    return true;
}

bool VideoDecoder::isHwDecoder() {
    bool isEvenEdge = isEven(mAvFrame->width) && isEven(mAvFrame->height);
    return mAvFrame->format == hw_pix_fmt || mAvFrame->format == AV_PIX_FMT_RGB24
           || (isEvenEdge && (mAvFrame->format ==
                              AV_PIX_FMT_YUV420P ||
                              mAvFrame->format ==
                              AV_PIX_FMT_NV12));
}

int VideoDecoder::decode(AVPacket *avPacket) {
    int64_t start = getCurrentTimeMs();
    // 主动塞到队列中的flush帧
    bool isEof = avPacket->size == 0 && avPacket->data == nullptr;
    int sendRes = avcodec_send_packet(mCodecContext, avPacket);
    int64_t sendPoint = getCurrentTimeMs() - start;

    bool isKeyFrame = avPacket->flags & AV_PKT_FLAG_KEY;
    LOGI(
            "[video] avcodec_send_packet...pts: %" PRId64 ", dts: %" PRId64 ", isKeyFrame: %d, res: %d, isEof: %d",
            avPacket->pts, avPacket->dts, isKeyFrame, sendRes, isEof)

    // avcodec_send_packet的-11表示要先读output，然后pkt需要重发
    mNeedResent = sendRes == AVERROR(EAGAIN);

    int receiveRes = AVERROR_EOF;
    int receiveCount = 0;
    do {
        start = getCurrentTimeMs();
        // avcodec_receive_frame的-11，表示需要发新帧
        receiveRes = avcodec_receive_frame(mCodecContext, mAvFrame);

        if (isEof && receiveRes != AVERROR_EOF && mRetryReceiveCount >= 0) {
            mNeedResent = true;
            mRetryReceiveCount--;
            LOGE("[video] send eof, not receive eof...retry count: %" PRId64,
                 mRetryReceiveCount)
        }

        if (receiveRes != 0) {
            LOGE("[video] avcodec_receive_frame err: %d, resent: %d, retry count: %" PRId64,
                 receiveRes, mNeedResent, mRetryReceiveCount)
            av_frame_unref(mAvFrame);
            // decode and receive frame arrived EOF
            if (isEof && receiveRes == AVERROR_EOF) {
                mRetryReceiveCount = RETRY_RECEIVE_COUNT;
            }
            // force EOF
            if (isEof && mRetryReceiveCount < 0) {
                receiveRes = AVERROR_EOF;
                mRetryReceiveCount = RETRY_RECEIVE_COUNT;
                mNeedResent = false;
            }
            break;
        }

        auto ptsMs = mAvFrame->pts * av_q2d(mFtx->streams[getStreamIndex()]->time_base) * 1000;
        LOGI(
                "[video] avcodec_receive_frame...pts: %" PRId64 ", time: %f, format: %d, need retry: %d",
                mAvFrame->pts, ptsMs, mAvFrame->format, mNeedResent)
        int64_t receivePoint = getCurrentTimeMs() - start;

        updateTimestamp(mAvFrame);

        if (mAvFrame->pts >= mSeekPos) {
            mSeekPos = INT64_MAX;
            mSeekEndTimeMs = getCurrentTimeMs();
            int64_t precisionSeekConsume = mSeekEndTimeMs - mSeekStartTimeMs;
            LOGE(
                    "[video] avcodec_receive_frame...pts: %" PRId64 ", precision seek consume: %" PRId64,
                    mAvFrame->pts, precisionSeekConsume)
        }

        bool isEvenEdge = isEven(mAvFrame->width) && isEven(mAvFrame->height);
        if (mAvFrame->format == hw_pix_fmt || mAvFrame->format == AV_PIX_FMT_RGB24
            || (isEvenEdge && (mAvFrame->format ==
                               AV_PIX_FMT_YUV420P ||
                               mAvFrame->format ==
                               AV_PIX_FMT_NV12))) {
            if (mOnFrameArrivedListener) {
                mOnFrameArrivedListener(mAvFrame);
            }
        } else if (mAvFrame->format !=
                   AV_PIX_FMT_NONE) { // mAvFrame->format == AV_PIX_FMT_YUV420P10LE先转为RGBA进行渲染
            AVFrame *swFrame = av_frame_alloc();
            int size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mAvFrame->width, mAvFrame->height,
                                                1);
            shadowedOutbuffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));
            av_image_fill_arrays(swFrame->data, swFrame->linesize, shadowedOutbuffer,
                                 AV_PIX_FMT_RGBA,
                                 mAvFrame->width, mAvFrame->height, 1);

            if (swsScale(mAvFrame, swFrame) > 0) {
                if (mOnFrameArrivedListener) {
                    mOnFrameArrivedListener(swFrame);
                }
            }

            av_frame_free(&swFrame);
            av_freep(&swFrame);
            av_free(shadowedOutbuffer);
        } else {
            LOGE("[video] frame format is AV_PIX_FMT_NONE")
        }

        av_frame_unref(mAvFrame);
        receiveCount++;

        LOGW(
                "[video] decode sendPoint: %" PRId64 ", receivePoint: %" PRId64 ", receiveCount: %d",
                sendPoint, receivePoint, receiveCount)
    } while (true);
    return receiveRes;
}

void VideoDecoder::showFrameToWindow(AVFrame *pFrame) {
    //硬解,并且配置直接关联surface,format为mediacoder
    if (isHwDecoder()) {
        auto startTime = std::chrono::steady_clock::now();
        //直接渲染到surface
        int result = av_mediacodec_release_buffer((AVMediaCodecBuffer *) (pFrame)->data[3], 1);
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        LOGI("video av_mediacodec_release_buffer 耗时:%f毫秒 result:%d", diffMilli, result);
        return;
    } else {
        auto startTime = std::chrono::steady_clock::now();
        //将yuv数据转成rgb

        //windowBuffer 入参出参,
        ANativeWindow_lock(nativeWindow, &windowBuffer, nullptr);

        //转成数组
        auto *dstWindow = static_cast<uint8_t *>(windowBuffer.bits);

        startTime = std::chrono::steady_clock::now();
        for (int i = 0; i < mCodecContext->height / scale; ++i) {
            //从rgbFrame->data中拷贝数据到window中
            //windowBuffer.stride * 4 === 像素*4个字节(rgba)
            memcpy(dstWindow + i * windowBuffer.stride * 4,
                   shadowedOutbuffer + i * pFrame->linesize[0],
                   pFrame->linesize[0]);
        }
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        LOGI("video rgb->显示 耗时:%f毫秒", diffMilli);
        ANativeWindow_unlockAndPost(nativeWindow);
    }


}

int VideoDecoder::swsScale(AVFrame *srcFrame, AVFrame *swFrame) {
    if (mSwsContext == nullptr) {
        mSwsContext = sws_getContext(srcFrame->width, srcFrame->height,
                                     AVPixelFormat(srcFrame->format),
                                     srcFrame->width, srcFrame->height, AV_PIX_FMT_RGBA,
                                     SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!mSwsContext) {
            return -1;
        }
    }

    // transform
    int ret = sws_scale(mSwsContext,
                        reinterpret_cast<const uint8_t *const *>(srcFrame->data),
                        srcFrame->linesize,
                        0,
                        srcFrame->height,
                        swFrame->data,
                        swFrame->linesize
    );

    // buffer is right, but some property lost
    if (swFrame->format == AV_PIX_FMT_NONE) {
        swFrame->format = AV_PIX_FMT_RGBA;
        swFrame->width = srcFrame->width;
        swFrame->height = srcFrame->height;
    }

    return ret;
}

int64_t VideoDecoder::getTimestamp() const {
    return mCurTimeStampMs;
}

void VideoDecoder::updateTimestamp(AVFrame *frame) {
    if (mStartTimeMsForSync < 0) {
        LOGE("update video start time")
        mStartTimeMsForSync = getCurrentTimeMs();
    }

    int64_t pts = 0;
    if (frame->pkt_dts != AV_NOPTS_VALUE) {
        pts = frame->pkt_dts;
    } else if (frame->pts != AV_NOPTS_VALUE) {
        pts = frame->pts;
    }
    // s -> ms
    mCurTimeStampMs = (int64_t) (pts * av_q2d(mTimeBase) * 1000);

    if (mFixStartTime) {
        mStartTimeMsForSync = getCurrentTimeMs() - mCurTimeStampMs;
        mFixStartTime = false;
        LOGE("fix video start time")
    }
}

int VideoDecoder::getWidth() const {
    return mWidth;
}

int VideoDecoder::getHeight() const {
    return mHeight;
}

int VideoDecoder::getFps() const {
    return mFps;
}

double VideoDecoder::getDuration() {
    return mDuration;
}

void VideoDecoder::avSync(AVFrame *frame) {
    int64_t elapsedTimeMs = getCurrentTimeMs() - mStartTimeMsForSync;
    int64_t diff = mCurTimeStampMs - elapsedTimeMs;
    diff = FFMIN(diff, DELAY_THRESHOLD);
    LOGI("[video] avSync, pts: %" PRId64 "ms, diff: %" PRId64 "ms", mCurTimeStampMs, diff)
    if (diff > 0) {
        av_usleep(diff * 1000);
    }
}

int VideoDecoder::seek(double pos) {
    flush();
    int64_t seekPos = av_rescale_q((int64_t) (pos * AV_TIME_BASE), AV_TIME_BASE_Q, mTimeBase);
    int ret = avformat_seek_file(mFtx, getStreamIndex(),
                                 INT64_MIN, seekPos, INT64_MAX,
                                 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
    LOGE("[video] seek to: %f, seekPos: %" PRId64 ", ret: %d", pos, seekPos, ret)
    // seek后需要恢复起始时间
    mFixStartTime = true;
    mSeekPos = seekPos;
    mSeekStartTimeMs = getCurrentTimeMs();
    return ret;
}

void VideoDecoder::release() {
    mFixStartTime = false;
    mStartTimeMsForSync = -1;
    if (mAvFrame != nullptr) {
        av_frame_free(&mAvFrame);
        av_freep(&mAvFrame);
        LOGI("av frame...release")
    }

    if (mSwsContext != nullptr) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
        LOGI("sws context...release")
    }

    if (mMediaCodecContext != nullptr) {
        av_mediacodec_default_free(mCodecContext);
        mMediaCodecContext = nullptr;
        LOGI("mediacodec context...release")
    }

    if (mHwDeviceCtx != nullptr) {
        av_buffer_unref(&mHwDeviceCtx);
        mHwDeviceCtx = nullptr;
        LOGI("hw device context...release")
    }

    if (mCodecContext != nullptr) {
        avcodec_free_context(&mCodecContext);
        mCodecContext = nullptr;
        LOGI("codec...release")
    }
}

int VideoDecoder::getRotate() {
    AVStream *stream = mFtx->streams[getStreamIndex()];
    return FFVideoReader::getRotate(stream);
}

