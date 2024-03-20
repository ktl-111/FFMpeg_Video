#include <bits/sysconf.h>
#include "VideoDecoder.h"
#include "../utils/loghelper.h"
#include "../utils/CommonUtils.h"
#include "../reader/FFVideoReader.h"
#include "libyuv/scale.h"
#include "libyuv/scale_argb.h"
#include "libyuv/rotate.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "../globals.h"
#include "libyuv/video_common.h"

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

VideoDecoder::VideoDecoder(int index, AVFormatContext *ftx) : BaseDecoder(index, ftx) {
    mSeekMutexObj = std::make_shared<MutexObj>();

    AVStream *pStream = mFtx->streams[getStreamIndex()];
    AVCodecParameters *params = pStream->codecpar;
    mRotate = FFVideoReader::getRotate(pStream);
    if (mRotate == 90 || mRotate == 270) {
        mWidth = params->height;
        mHeight = params->width;
    } else {
        mWidth = params->width;
        mHeight = params->height;
    }
    mFps = av_q2d(pStream->avg_frame_rate);
    LOGI("VideoDecoder() %d*%d rotate:%d", mWidth, mHeight, mRotate)
}

VideoDecoder::~VideoDecoder() {
    release();
}

void VideoDecoder::setSurface(jobject surface) {
    mSurface = surface;
}

void VideoDecoder::setOutConfig(const std::shared_ptr<OutConfig> outConfig) {
    mOutConfig = outConfig;
    LOGI("setOutConfig %d*%d src %d*%d", mOutConfig->getWidth(), mOutConfig->getHeight(),
         (outConfig)->getWidth(), (outConfig)->getHeight())
}

bool VideoDecoder::prepare(JNIEnv *env) {
    AVStream *stream = mFtx->streams[getStreamIndex()];

    AVCodecParameters *params = stream->codecpar;

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

    if (false) {
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
            mVideoCodec = avcodec_find_decoder(params->codec_id);
            LOGE("not find %s,find %s", mediacodecName.c_str(), mVideoCodec->name)
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

        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         getCropWidth() != 0 ? getCropWidth() : getWidth(),
                                         getCropHeight() != 0 ? getCropHeight() : getHeight(),
                                         WINDOW_FORMAT_RGBA_8888);
    }
    // 根据设备核心数设置线程数
    long threadCount = sysconf(_SC_NPROCESSORS_ONLN);

    mCodecContext->thread_count = threadCount > 0 ? threadCount : 1;
    // open codec
    int ret = avcodec_open2(mCodecContext, mVideoCodec, nullptr);
    if (ret != 0) {
        std::string msg = "codec open failed";
        if (mErrorMsgListener) {
            mErrorMsgListener(-3000, msg);
        }
        return false;
    }

    mStartTimeMsForSync = -1;
    LOGI("[video] prepare name: %s,mFps: %f,threadCount:%ld,%d*%d", mVideoCodec->name, mFps,
         threadCount, mWidth, mHeight)

    return true;
}

void VideoDecoder::surfaceReCreate(JNIEnv *env, jobject surface) {
    LOGI("[video] surfaceReCreate")
    mSurface = surface;
    if (mHwDeviceCtx) {
        if (mSurface != nullptr) {
            mMediaCodecContext = av_mediacodec_alloc_context();
            av_mediacodec_default_init(mCodecContext, mMediaCodecContext, mSurface);
        }
    } else {
        //创建nativewindow
        nativeWindow = ANativeWindow_fromSurface(env, mSurface);
        //修改缓冲区格式和大小,对应视频格式和大小
        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         getCropWidth() != 0 ? getCropWidth() : getWidth(),
                                         getCropHeight() != 0 ? getCropHeight() : getHeight(),
                                         WINDOW_FORMAT_RGBA_8888);
        if (dstWindowBuffer) {
            //windowBuffer 入参出参,
            int result = ANativeWindow_lock(nativeWindow, &windowBuffer, nullptr);
            LOGI("surfaceReCreate ANativeWindow_lock %d %s stride:%d %d*%d", result,
                 av_err2str(result),
                 windowBuffer.stride, windowBuffer.width, windowBuffer.height)
            memcpy(windowBuffer.bits, dstWindowBuffer,
                   windowBuffer.stride * windowBuffer.height * 4);

            result = ANativeWindow_unlockAndPost(nativeWindow);
            LOGI("surfaceReCreate ANativeWindow_unlockAndPost %d %s", result, av_err2str(result))
        }
    }
}

void VideoDecoder::surfaceDestroy(JNIEnv *env) {
    LOGI("[video] surfaceDestroy")
    mSurface = nullptr;
    if (mHwDeviceCtx) {
        av_mediacodec_default_free(mCodecContext);
        mMediaCodecContext = nullptr;
    } else {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
}

bool VideoDecoder::isHwDecoder(AVFrame *frame) {
    return frame->format == hw_pix_fmt;
}

int VideoDecoder::decode(AVPacket *avPacket, AVFrame *frame) {
    // 主动塞到队列中的flush帧
    LOGI("decode start pts:%ld dts:%ld", avPacket->pts, avPacket->dts)
    int sendRes = -1;
    int receiveRes = -1;

    AVFrame *pAvFrame = av_frame_alloc();
    bool isEof = avPacket->size == 0 && avPacket->data == nullptr;
    sendRes = -1;
    receiveRes = -1;
    sendRes = avcodec_send_packet(mCodecContext, avPacket);
    mNeedResent = sendRes == AVERROR(EAGAIN) || isEof;
    bool isKeyFrame = avPacket->flags & AV_PKT_FLAG_KEY;
    LOGI(
            "[video] avcodec_send_packet...pts: %" PRId64 ", dts: %" PRId64 ", isKeyFrame: %d, res: %d, isEof: %d,extra_hw_frames:%d",
            avPacket->pts, avPacket->dts, isKeyFrame, sendRes, isEof,
            mCodecContext->extra_hw_frames)
    // avcodec_receive_frame的-11，表示需要发新帧
    receiveRes = avcodec_receive_frame(mCodecContext, pAvFrame);

    LOGI("[video] avcodec_receive_frame %d %s", receiveRes, av_err2str(receiveRes))

    if (receiveRes != 0) {
        LOGE("[video] avcodec_receive_frame err: %d, resent: %d", receiveRes, mNeedResent)
        av_frame_free(&pAvFrame);
        // force EOF
        if (isEof && receiveRes == AVERROR_EOF) {
            receiveRes = AVERROR_EOF;
            mNeedResent = false;
        }
        return receiveRes;
    }

    auto ptsMs = pAvFrame->pts * av_q2d(mFtx->streams[getStreamIndex()]->time_base) * 1000;
    LOGI(
            "[video] avcodec_receive_frame...pts: %" PRId64 ", time: %f, format: %d, need retry: %d",
            pAvFrame->pts, ptsMs, pAvFrame->format, mNeedResent)

    AVFrame *srcFrame = nullptr;
    bool isRotate = mRotate != 0;
    if (isRotate) {
        srcFrame = av_frame_alloc();
        if (mRotate == 90 || mRotate == 270) {
            srcFrame->width = pAvFrame->height;
            srcFrame->height = pAvFrame->width;
        } else {
            srcFrame->width = pAvFrame->width;
            srcFrame->height = pAvFrame->height;
        }
        srcFrame->pts = pAvFrame->pts;
        srcFrame->pkt_dts = pAvFrame->pkt_dts;
        srcFrame->duration = pAvFrame->duration;
        srcFrame->pkt_size = pAvFrame->pkt_size;
        srcFrame->format = pAvFrame->format;
        int ret = av_frame_get_buffer(srcFrame, 0);
        LOGI("[video] decode av_frame_get_buffer %d", ret)
        ret = libyuv::I420Rotate(pAvFrame->data[0], pAvFrame->linesize[0],
                                 pAvFrame->data[1], pAvFrame->linesize[1],
                                 pAvFrame->data[2], pAvFrame->linesize[2],
                                 srcFrame->data[0], srcFrame->linesize[0],
                                 srcFrame->data[1], srcFrame->linesize[1],
                                 srcFrame->data[2], srcFrame->linesize[2],
                                 pAvFrame->width, pAvFrame->height,
                                 libyuv::RotationMode(mRotate));
        LOGI("[video] decode I420Rotate %d %d*%d", ret, srcFrame->width, srcFrame->height)
        av_frame_free(&pAvFrame);
    } else {
        srcFrame = pAvFrame;
    }

    int dstWidth;
    int dstHeight;
    int cropWidth = 0;
    int cropHeight = 0;

    if (mOutConfig) {
        dstWidth = getWidth();
        dstHeight = getHeight();
        cropWidth = mOutConfig->getCropWidth();
        cropHeight = mOutConfig->getCropHeight();
    } else {
        dstWidth = srcFrame->width;
        dstHeight = srcFrame->height;
    }
    bool needScale = false;
    if (dstWidth != srcFrame->width || dstHeight != srcFrame->height) {
        needScale = true;
    }
    LOGI("[video] decode needScale:%d %d*%d", needScale, dstWidth, dstHeight)
    if (needScale) {
        AVFrame *scaleFrame = av_frame_alloc();
        scaleFrame->width = dstWidth;
        scaleFrame->height = dstHeight;
        scaleFrame->format = srcFrame->format;

        scaleFrame->pts = srcFrame->pts;
        scaleFrame->pkt_dts = srcFrame->pkt_dts;
        scaleFrame->duration = srcFrame->duration;
        scaleFrame->pkt_size = srcFrame->pkt_size;
        int ret = av_frame_get_buffer(scaleFrame, 0);
        if (srcFrame->format == AV_PIX_FMT_BGRA) {
            ret = libyuv::ARGBScale(srcFrame->data[0], srcFrame->linesize[0],
                                    srcFrame->width, srcFrame->height,
                                    scaleFrame->data[0], scaleFrame->linesize[0],
                                    dstWidth, dstHeight, libyuv::kFilterNone);
        } else {
            ret = libyuv::I420Scale(srcFrame->data[0], srcFrame->linesize[0],
                                    srcFrame->data[1], srcFrame->linesize[1],
                                    srcFrame->data[2], srcFrame->linesize[2],
                                    srcFrame->width, srcFrame->height,
                                    scaleFrame->data[0], scaleFrame->linesize[0],
                                    scaleFrame->data[1], scaleFrame->linesize[1],
                                    scaleFrame->data[2], scaleFrame->linesize[2],
                                    dstWidth, dstHeight, libyuv::kFilterNone);
        }
        LOGI("[video] decode Scale:%d", ret)
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = scaleFrame;
    }
    bool needCrop = false;
    if (cropWidth != 0 && cropHeight != 0 &&
        (cropWidth < srcFrame->width || cropHeight < srcFrame->height)) {
        needCrop = true;
    }
    if (needCrop) {
        int srcWidth = srcFrame->width;
        int srcHeight = srcFrame->height;
        AVFrame *cropFrame = av_frame_alloc();
        cropFrame->width = cropWidth;
        cropFrame->height = cropHeight;
        cropFrame->format = srcFrame->format;

        cropFrame->pts = srcFrame->pts;
        cropFrame->pkt_dts = srcFrame->pkt_dts;
        cropFrame->duration = srcFrame->duration;
        int ret = av_frame_get_buffer(cropFrame, 0);

        int diffWidth = srcWidth - cropWidth;
        int dx = diffWidth / 2 + diffWidth % 2;

        int diffHeight = srcHeight - cropHeight;
        int dy = diffHeight / 2 + diffHeight % 2;
        LOGI("[video] decode av_frame_get_buffer:%d src:%d*%d d:%d-%d crop:%d*%d", ret,
             srcWidth,
             srcHeight, dx, dy,
             cropWidth, cropHeight)

        int size = av_image_get_buffer_size((AVPixelFormat) srcFrame->format,
                                            srcFrame->width, srcFrame->height, 1);
        auto *buffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));

        ret = av_image_copy_to_buffer(buffer, size, srcFrame->data, srcFrame->linesize,
                                      (AVPixelFormat) srcFrame->format,
                                      srcFrame->width, srcFrame->height, 1);

        LOGI("[video] decode av_image_copy_to_buffer %d %d", ret, size)
        if (srcFrame->format == AV_PIX_FMT_BGRA) {
            ret = libyuv::ConvertToARGB(buffer, size,
                                        cropFrame->data[0], cropFrame->linesize[0],
                                        dx, dy,
                                        srcWidth, srcHeight,
                                        cropWidth, cropHeight,
                                        libyuv::kRotate0, libyuv::FOURCC_ARGB
            );
        } else {
            ret = libyuv::ConvertToI420(buffer, size,
                                        cropFrame->data[0], cropFrame->linesize[0],
                                        cropFrame->data[1], cropFrame->linesize[1],
                                        cropFrame->data[2], cropFrame->linesize[2],
                                        dx, dy,
                                        srcWidth, srcHeight,
                                        cropWidth, cropHeight,
                                        libyuv::kRotate0, libyuv::FOURCC_I420
            );
        }
        LOGI("[video] decode Convert:%d", ret)
        av_free(buffer);
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = cropFrame;
    }

    int result = av_frame_ref(frame, srcFrame);
    frame->time_base = mTimeBase;
//    }

    LOGI("[video] decode done result:%d ,pts:%ld(%f) copy:%d %s format:%s %d*%d", result,
         frame->pts,
         frame->pts * av_q2d(frame->time_base),
         0, av_err2str(0), av_get_pix_fmt_name((AVPixelFormat) frame->format),
         frame->width, frame->height)
    av_frame_free(&srcFrame);
    return receiveRes;
}

void VideoDecoder::resultCallback(AVFrame *srcFrame) {
    updateTimestamp(srcFrame);
    int dstWidth = srcFrame->width;
    int dstHeight = srcFrame->height;
    LOGI("resultCallback pts:%ld(%lf) format:%s %d*%d", srcFrame->pts,
         srcFrame->pts * av_q2d(srcFrame->time_base),
         av_get_pix_fmt_name((AVPixelFormat) srcFrame->format), dstWidth, dstHeight)
    if (isHwDecoder(srcFrame)) {
        if (mOnFrameArrivedListener) {
            mOnFrameArrivedListener(srcFrame);
        }
    } else { // mAvFrame->format == AV_PIX_FMT_YUV420P10LE先转为RGBA进行渲染
        AVFrame *dstFrame = av_frame_alloc();
        dstFrame->format = AV_PIX_FMT_RGBA;
        dstFrame->width = dstWidth;
        dstFrame->height = dstHeight;
        av_frame_get_buffer(dstFrame, 0);
        dstFrame->pts = srcFrame->pts;
        dstFrame->duration = srcFrame->duration;
        dstFrame->time_base = srcFrame->time_base;
        if (swsScale(srcFrame, dstFrame) > 0) {
            LOGI("resultCallback swsScale done")
            if (mOnFrameArrivedListener) {
                mOnFrameArrivedListener(dstFrame);
            }
            LOGI("resultCallback swsScale end")
        } else {
            LOGE("showFrameToWindow swsScale fail")
        }
        LOGI("resultCallback frame:%p", &dstFrame)
        av_frame_free(&dstFrame);
    }

}

void VideoDecoder::showFrameToWindow(AVFrame *pFrame) {
    //硬解,并且配置直接关联surface,format为mediacoder
    LOGI("showFrameToWindow pts:%ld(%lf) format:%s data:%p %d %d*%d", pFrame->pts,
         pFrame->pts * av_q2d(pFrame->time_base),
         av_get_pix_fmt_name((AVPixelFormat) pFrame->format), &pFrame->data[3], pFrame->linesize[0],
         pFrame->width, pFrame->height)
    if (isHwDecoder(pFrame)) {
        auto startTime = std::chrono::steady_clock::now();
        //直接渲染到surface
        LOGI("video av_mediacodec_release_buffer start");
        int result = av_mediacodec_release_buffer((AVMediaCodecBuffer *) (pFrame)->data[3], 1);
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        LOGI("video av_mediacodec_release_buffer 耗时:%f毫秒 result:%d", diffMilli, result);
        return;
    } else {
        auto startTime = std::chrono::steady_clock::now();
        //将yuv数据转成rgb

        //windowBuffer 入参出参,
        int result = ANativeWindow_lock(nativeWindow, &windowBuffer, nullptr);
        LOGI("showFrameToWindow ANativeWindow_lock %d %s stride:%d %d*%d", result,
             av_err2str(result),
             windowBuffer.stride, windowBuffer.width, windowBuffer.height)
        if (dstWindowBuffer) {
            free(dstWindowBuffer);
        }
        dstWindowBuffer = static_cast<uint8_t *>(malloc(
                windowBuffer.stride * windowBuffer.height * 4));
        //转成数组
        uint8_t *buffer = static_cast<uint8_t *>(windowBuffer.bits);
        startTime = std::chrono::steady_clock::now();
        for (int i = 0; i < pFrame->height; ++i) {
            //从rgbFrame->data中拷贝数据到window中
            //windowBuffer.stride * 4 === 像素*4个字节(rgba)
            memcpy(buffer + i * windowBuffer.stride * 4,
                   pFrame->data[0] + i * pFrame->linesize[0],
                   pFrame->linesize[0]);
            memcpy(dstWindowBuffer + i * windowBuffer.stride * 4,
                   pFrame->data[0] + i * pFrame->linesize[0],
                   pFrame->linesize[0]);
        }
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        LOGI("video rgb->显示 耗时:%f毫秒 pts:%ld(%f)", diffMilli, pFrame->pts,
             pFrame->pts * av_q2d(pFrame->time_base))
        result = ANativeWindow_unlockAndPost(nativeWindow);
        LOGI("showFrameToWindow ANativeWindow_unlockAndPost %d %s", result, av_err2str(result))
    }
}

int VideoDecoder::swsScale(AVFrame *srcFrame, AVFrame *dstFrame) {
    if (mSwsContext == nullptr) {
        LOGI("swsScale %d,%d,%s   %d,%d,%s", srcFrame->width, srcFrame->height,
             av_get_pix_fmt_name(AVPixelFormat(srcFrame->format)), dstFrame->width,
             dstFrame->height,
             av_get_pix_fmt_name(AVPixelFormat(dstFrame->format)))
        mSwsContext = sws_getContext(srcFrame->width, srcFrame->height,
                                     AVPixelFormat(srcFrame->format),
                                     dstFrame->width, dstFrame->height,
                                     AVPixelFormat(dstFrame->format),
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
                        dstFrame->data,
                        dstFrame->linesize
    );

    return ret;
}

int64_t VideoDecoder::getTimestamp() const {
    return mCurTimeMs;
}

bool VideoDecoder::initFilter() {
    //设置filter
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!buffersrc || !buffersink || !filter_graph) {
        LOGE("initFilter buffersrc  buffersink avfilter_graph_alloc fail")
        return -1;
    }

    char args[512];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             getCodecContext()->width, getCodecContext()->height, getCodecContext()->pix_fmt,
             getTimeBase().num, getTimeBase().den,
             getCodecContext()->sample_aspect_ratio.num,
             getCodecContext()->sample_aspect_ratio.den);
    LOGI("initFilter args %s", args)
    int result = avfilter_graph_create_filter(&mBuffersrcContext, buffersrc, "in",
                                              args, NULL, filter_graph);

    if (result != 0) {
        LOGE("initFilter buffersrc fail,%d %s", result, av_err2str(result))
        return false;
    }

    result = avfilter_graph_create_filter(&mBuffersinkContext, buffersink, "out",
                                          NULL, NULL, filter_graph);
    if (result != 0) {
        LOGE("initFilter buffersink fail,%d %s", result, av_err2str(result))
        return false;
    }
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    //in->fps filter的输入端口名称
    //outputs->源头的输出
    outputs->name = av_strdup("in");
    outputs->filter_ctx = mBuffersrcContext;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    //out->fps filter的输出端口名称
    //outputs->filter后的输入
    inputs->name = av_strdup("out");
    inputs->filter_ctx = mBuffersinkContext;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    const char *fpsTag = "fps=";
    char *fpsFilter = new char[strlen(fpsTag), sizeof(mOutConfig->getFps())];
    sprintf(fpsFilter, "%s%f", fpsTag, mOutConfig->getFps());
    LOGI("initFilter fpsfilter:%s", fpsFilter)
    //buffer->输出(outputs)->filter in(av_strdup("in")->fps filter->filter out(av_strdup("out"))->输入(inputs)->buffersink
    result = avfilter_graph_parse_ptr(filter_graph, fpsFilter, &inputs, &outputs, NULL);
    if (result != 0) {
        LOGE("checkFrame avfilter_graph_parse_ptr fail,%d %s", result, av_err2str(result))
        return false;
    }
    result = avfilter_graph_config(filter_graph, NULL);
    if (result != 0) {
        LOGE("initFilter avfilter_graph_config fail,%d %s", result, av_err2str(result))
        return false;
    }
    LOGI("initFilter success")
    return true;
}

void VideoDecoder::seekLock() {
    mSeekMutexObj->lock();
}

void VideoDecoder::seekUnlock() {
    mSeekMutexObj->unlock();
}

void VideoDecoder::updateTimestamp(AVFrame *frame) {
    if (mStartTimeMsForSync < 0) {
        LOGE("update video start time")
        mStartTimeMsForSync = getCurrentTimeMs();
    }

    int64_t pts = 0;
    if (frame->pts != AV_NOPTS_VALUE) {
        pts = frame->pts;
    } else if (frame->pkt_dts != AV_NOPTS_VALUE) {
        pts = frame->pkt_dts;
    }
    // s -> ms
    mCurTimeMs = (int64_t) (pts * av_q2d(frame->time_base) * 1000);
    LOGI("updateTimestamp updateTimestamp:%ld pts:%ld", mCurTimeMs, pts)
    if (mFixStartTime) {
        mStartTimeMsForSync = getCurrentTimeMs() - mCurTimeMs;
        mFixStartTime = false;
        LOGE("fix video start time")
    }
}

int VideoDecoder::getWidth() const {
    if (mOutConfig) {
        return mOutConfig->getWidth();
    }
    return mWidth;
}


int VideoDecoder::getHeight() const {
    if (mOutConfig) {
        return mOutConfig->getHeight();
    }
    return mHeight;
}

double VideoDecoder::getFps() const {
//    if (mOutConfig) {
//        return mOutConfig->getFps();
//    }
    return mFps;
}

int VideoDecoder::getCropWidth() {
    if (mOutConfig) {
        return mOutConfig->getCropWidth();
    }
    return 0;
}

int VideoDecoder::getCropHeight() {
    if (mOutConfig) {
        return mOutConfig->getCropHeight();
    }
    return 0;
}

double VideoDecoder::getDuration() {
    return mDuration;
}

void VideoDecoder::avSync(AVFrame *frame) {
    int64_t elapsedTimeMs = getCurrentTimeMs() - mStartTimeMsForSync;
    int64_t diff = mCurTimeMs - elapsedTimeMs;
    diff = FFMIN(diff, DELAY_THRESHOLD);
    LOGI("[video] avSync, pts: %" PRId64 "ms, diff: %" PRId64 "ms", mCurTimeMs, diff)
    if (diff > 0) {
        av_usleep(diff * 1000);
    }
}

int VideoDecoder::seek(int64_t pos) {
    int64_t seekPos = (int64_t) (pos / av_q2d(getTimeBase())) / 1000;
    int ret = avformat_seek_file(mFtx, getStreamIndex(),
                                 INT64_MIN, seekPos, INT64_MAX,
                                 0);
    flush();
    LOGE("[video] seek to: %ld, seekPos: %" PRId64 ", ret: %d", pos, seekPos, ret)
    // seek后需要恢复起始时间
    mFixStartTime = true;
    return ret;
}

void VideoDecoder::release() {
    mFixStartTime = false;
    mStartTimeMsForSync = -1;

    if (mSwsContext != nullptr) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
        LOGI("sws context...release")
    }
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
        mSurface = nullptr;
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
    mOutConfig = nullptr;
    if (mBuffersrcContext) {
        avfilter_free(mBuffersrcContext);
        mBuffersrcContext = nullptr;
    }
    if (mBuffersinkContext) {
        avfilter_free(mBuffersinkContext);
        mBuffersinkContext = nullptr;
    }
}