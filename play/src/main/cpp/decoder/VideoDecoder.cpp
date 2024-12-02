#include <bits/sysconf.h>
#include "VideoDecoder.h"
#include "Logger.h"
#include "../utils/CommonUtils.h"
#include "../reader/FFVideoReader.h"
#include "libyuv/scale.h"
#include "libyuv/scale_argb.h"
#include "libyuv/rotate.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "../globals.h"
#include "libyuv/video_common.h"
#include "VideoFrameUtil.h"
#include "CodecUtils.h"

VideoDecoder::VideoDecoder(int index, AVFormatContext *ftx) : BaseDecoder(index, ftx) {
    mSeekMutexObj = std::make_shared<MutexObj>();
    mSurfaceMutexObj = std::make_shared<MutexObj>();

    AVStream *pStream = getStream();
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
    LOGI("VideoDecoder() init %d*%d rotate:%d fps:%f", mWidth, mHeight, mRotate, mFps)
}

VideoDecoder::~VideoDecoder() {
    release();
}

void VideoDecoder::initConfig(JNIEnv *env, jobject out_config) {
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

        int videoWidth = getWidth();
        int videoHeight = getHeight();

        if ((outWidth != 0 && outHeight != 0) && videoWidth * videoHeight > outWidth * outHeight) {
            float shrinkPercent = outWidth * outHeight / (videoWidth * videoHeight * 1.0f);
            outWidth = videoWidth * shrinkPercent;
            outHeight = videoHeight * shrinkPercent;
            LOGI("shrinkPercent :%f", shrinkPercent)
        } else {
            outWidth = videoWidth;
            outHeight = videoHeight;
        }

        if (cropHeight != 0 && cropWidth != 0) {
            if (outWidth < cropWidth) {
                outWidth = cropWidth;
                outHeight = (jint) (1.0 * outWidth * videoHeight / videoWidth);
                outHeight += outHeight % 2;
            }
            if (outHeight < cropHeight) {
                outHeight = cropHeight;
                outWidth = (jint) (1.0 * outHeight * videoWidth / videoHeight);
                outWidth += outWidth % 2;
            }

        }

        outWidth += outWidth % 2;
        outHeight += outHeight % 2;

        outConfig = std::make_shared<OutConfig>(outWidth, outHeight, cropWidth, cropHeight,
                                                outFps);

        LOGI("set out config,video:%d*%d,out:%d*%d,crop:%d*%d,fps:%f",
             videoWidth, videoHeight,
             outWidth, outHeight, cropWidth, cropHeight, outFps)
    } else {
        LOGI("not out config")
    }
    setOutConfig(outConfig);
}

void VideoDecoder::setSurface(jobject surface) {
    mSurface = surface;
}

void VideoDecoder::setOutConfig(const std::shared_ptr<OutConfig> outConfig) {
    mOutConfig = outConfig;
}

bool VideoDecoder::prepare(JNIEnv *env) {
    AVStream *stream = getStream();

    AVCodecParameters *params = stream->codecpar;

    mVideoCodec = CodecUtils::findDecodec(params->codec_id, false);

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
    if (mSurface) {
        //创建nativewindow
        nativeWindow = ANativeWindow_fromSurface(env, mSurface);
        //修改缓冲区格式和大小,对应视频格式和大小

        ANativeWindow_setBuffersGeometry(nativeWindow, getTargetWidth(), getTargetHeight(),
                                         WINDOW_FORMAT_RGBA_8888);
    }
    // 根据设备核心数设置线程数
    long threadCount = sysconf(_SC_NPROCESSORS_ONLN);
    mCodecContext->skip_frame = AVDISCARD_NONREF;
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
    LOGI("[video] prepare name:%s,threadCount:%ld,%d*%d,mFps:%f", mVideoCodec->name,
         threadCount, mWidth, mHeight, mFps)
    initFilter();
    return true;
}

void VideoDecoder::releaseFilter() {
    if (buffersinkContext) {
        avfilter_free(buffersinkContext);
        buffersinkContext = nullptr;
    }
    if (buffersrcContext) {
        avfilter_free(buffersrcContext);
        buffersrcContext = nullptr;
    }
    if (inputs) {
        avfilter_inout_free(&inputs);
        inputs = nullptr;
    }
    if (outputs) {
        avfilter_inout_free(&outputs);
        outputs = nullptr;
    }
    if (filter_graph) {
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
    }
}

void VideoDecoder::initFilter() {
    int result = 0;
    //设置filter
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    filter_graph = avfilter_graph_alloc();

    if (!buffersrc || !buffersink || !filter_graph) {
        LOGE("initFilter buffersrc  buffersink avfilter_graph_alloc fail")
        return;
    }
    char args[512];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             getWidth(), getHeight(), getCodecContext()->pix_fmt,
             getTimeBase().num, getTimeBase().den,
             getCodecContext()->sample_aspect_ratio.num,
             getCodecContext()->sample_aspect_ratio.den);
    LOGI("initFilter args %s", args)
    result = avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in",
                                          args, NULL, filter_graph);

    if (result != 0) {
        LOGE("initFilter buffersrc fail,%d %s", result, av_err2str(result))
        return;
    }

    result = avfilter_graph_create_filter(&buffersinkContext, buffersink, "out",
                                          NULL, NULL, filter_graph);
    if (result != 0) {
        LOGE("initFilter buffersink fail,%d %s", result, av_err2str(result))
        return;
    }
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    //in->fps filter的输入端口名称
    //outputs->源头的输出
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrcContext;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    //out->fps filter的输出端口名称
    //outputs->filter后的输入
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersinkContext;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    int fps = getTargetFps();
    char *fpsFilter = new char[sizeof(fps)];
    sprintf(fpsFilter, "%d", fps);
    const AVFilter *pFilter = avfilter_get_by_name("fps");
    AVFilterContext *fpsContext;
    avfilter_graph_create_filter(&fpsContext, pFilter, "fps",
                                 fpsFilter, NULL, filter_graph);
    //    //buffer->输出(outputs)->filter in(av_strdup("in")->fps filter->filter out(av_strdup("out"))->输入(inputs)->buffersink
    avfilter_link(buffersrcContext, 0, fpsContext, 0);
    avfilter_link(fpsContext, 0, buffersinkContext, 0);

    //    result = avfilter_graph_parse_ptr(filter_graph, "fps=30", &inputs, &outputs, NULL);
    //    if (result != 0) {
    //        LOGE("initFilter avfilter_graph_parse_ptr fail,%d %s", result, av_err2str(result))
    //        return;
    //    }
    result = avfilter_graph_config(filter_graph, NULL);
    if (result != 0) {
        LOGE("initFilter avfilter_graph_config fail,%d %s", result, av_err2str(result))
    }
    LOGI("initFilter done")
}

void VideoDecoder::surfaceReCreate(JNIEnv *env, jobject surface) {
    LOGI("[video] surfaceReCreate")
    mSurface = surface;
    //创建nativewindow
    nativeWindow = ANativeWindow_fromSurface(env, mSurface);
    //修改缓冲区格式和大小,对应视频格式和大小
    ANativeWindow_setBuffersGeometry(nativeWindow, getTargetWidth(), getTargetHeight(),
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

void VideoDecoder::surfaceDestroy(JNIEnv *env) {
    LOGI("[video] surfaceDestroy")
    mSurface = nullptr;
    ANativeWindow_release(nativeWindow);
    nativeWindow = nullptr;
}

int VideoDecoder::convertFrameTo420Frame(AVFrame *srcFrame, AVFrame *dstFrame) {
    if (mTransformContext == nullptr) {
        LOGI("convertFrameTo420Frame %d,%d,%s   %d,%d,%s", srcFrame->width, srcFrame->height,
             av_get_pix_fmt_name(AVPixelFormat(srcFrame->format)), dstFrame->width,
             dstFrame->height,
             av_get_pix_fmt_name(AVPixelFormat(dstFrame->format)))
        mTransformContext = sws_getContext(srcFrame->width, srcFrame->height,
                                           AVPixelFormat(srcFrame->format),
                                           dstFrame->width, dstFrame->height,
                                           AVPixelFormat(dstFrame->format),
                                           SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!mTransformContext) {
            return -1;
        }
    }

    // transform
    int ret = sws_scale(mTransformContext,
                        reinterpret_cast<const uint8_t *const *>(srcFrame->data),
                        srcFrame->linesize,
                        0,
                        srcFrame->height,
                        dstFrame->data,
                        dstFrame->linesize
    );

    return ret;
}

int VideoDecoder::decode(AVPacket *avPacket, AVFrame *frame) {
    // 主动塞到队列中的flush帧
    LOGI("decode start pts:%ld(%f) dts:%ld", avPacket->pts,
         avPacket->pts * av_q2d(getTimeBase()),
         avPacket->dts)
    int sendRes = -1;
    int receiveRes = -1;

    AVFrame *pAvFrame = av_frame_alloc();
    bool isEof = avPacket->size == 0 && avPacket->data == nullptr;
    sendRes = -1;
    receiveRes = -1;
    sendRes = avcodec_send_packet(mCodecContext, avPacket);
    mNeedResent = sendRes == AVERROR(EAGAIN) || isEof;
    bool isKeyFrame = avPacket->flags & AV_PKT_FLAG_KEY;
    LOGI("[video] avcodec_send_packet...pts: %" PRId64 "(%f), dts: %" PRId64 ", isKeyFrame: %d, res: %d, isEof: %d",
         avPacket->pts, avPacket->pts * av_q2d(getStream()->time_base) * 1000, avPacket->dts,
         isKeyFrame, sendRes, isEof)
    // avcodec_receive_frame的-11，表示需要发新帧
    receiveRes = avcodec_receive_frame(mCodecContext, pAvFrame);

    LOGI("[video] avcodec_receive_frame %d %s", receiveRes, av_err2str(receiveRes))

    AVFrame *filtered_frame = nullptr;
    if (receiveRes != 0) {
        LOGI("[video] avcodec_receive_frame err: %d, resent: %d", receiveRes, mNeedResent)
        av_frame_free(&pAvFrame);
        if (isEof && receiveRes == AVERROR_EOF) {
            receiveRes = AVERROR_EOF;
            mNeedResent = false;
        }
        if (receiveRes == AVERROR_EOF) {
            //需要check filter里还有无数据
            filtered_frame = av_frame_alloc();
            int frameFlags = av_buffersrc_add_frame_flags(buffersrcContext, nullptr,
                                                          AV_BUFFERSRC_FLAG_PUSH);
            int buffersinkGetFrame = av_buffersink_get_frame(buffersinkContext, filtered_frame);
            LOGI("decode AVERROR_EOF filter frame %d %d ", frameFlags, buffersinkGetFrame)
            if (buffersinkGetFrame == AVERROR_EOF) {
                mNeedResent = false;
                return AVERROR_EOF;
            }
            receiveRes = 0;
            mNeedResent = false;
        } else {
            return receiveRes;
        }
    } else {
        LOGI("decode sendFilter %ld(%f)", pAvFrame->pts, pAvFrame->pts * av_q2d(getTimeBase()))
        int64_t pts = pAvFrame->pts;
        int keyFrame = pAvFrame->key_frame;
        AVPictureType type = pAvFrame->pict_type;
        filtered_frame = av_frame_alloc();
        // 将帧发送到filter图中
        int frameFlags = av_buffersrc_add_frame_flags(buffersrcContext, pAvFrame,
                                                      AV_BUFFERSRC_FLAG_PUSH);
        int buffersinkGetFrame = av_buffersink_get_frame(buffersinkContext, filtered_frame);
        if (frameFlags < 0 || buffersinkGetFrame < 0) {
            LOGI("decode filter frame %d %d pts:%ld(%f) %d %d", frameFlags, buffersinkGetFrame,
                 pts, pts * av_q2d(getTimeBase()), keyFrame, type);
            av_frame_free(&pAvFrame);
            av_frame_free(&filtered_frame);
            if (buffersinkGetFrame == AVERROR_EOF) {
                buffersinkGetFrame = AVERROR_EOF;
                mNeedResent = false;
            }
            return buffersinkGetFrame;
        }
    }

    AVRational outTimeBase = {1, (int) getTargetFps() * TimeBaseDiff};

    filtered_frame->pts = filtered_frame->pts * TimeBaseDiff;
    filtered_frame->pkt_dts = filtered_frame->pts - TimeBaseDiff;
    filtered_frame->time_base = outTimeBase;
    filtered_frame->pkt_duration = TimeBaseDiff;
    convertFrame(filtered_frame, frame);

    LOGI("decode convertFrame %ld(%f)  format:%s %d",
         frame->pts,
         frame->pts * av_q2d(outTimeBase),
         av_get_pix_fmt_name((AVPixelFormat) frame->format), frame->pict_type)

    return receiveRes;
}

void VideoDecoder::convertFrame(AVFrame *srcFrame, AVFrame *dstFrame) {
    auto ptsMs = srcFrame->pts * av_q2d(srcFrame->time_base);
    LOGI("convertFrame avcodec_receive_frame...pts: %" PRId64 "(%f), format: %s, need retry: %d",
         srcFrame->pts, ptsMs, av_get_pix_fmt_name((AVPixelFormat) srcFrame->format),
         mNeedResent)

    if (srcFrame->format != AV_PIX_FMT_YUV420P) {
        AVFrame *converSrcFrame = av_frame_alloc();
        converSrcFrame->format = AV_PIX_FMT_YUV420P;
        converSrcFrame->width = srcFrame->width;
        converSrcFrame->height = srcFrame->height;
        av_frame_get_buffer(converSrcFrame, 0);
        converSrcFrame->pts = srcFrame->pts;
        converSrcFrame->pkt_duration = srcFrame->pkt_duration;
        converSrcFrame->time_base = srcFrame->time_base;
        int ret = convertFrameTo420Frame(srcFrame, converSrcFrame);
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = converSrcFrame;
    }

    if (mRotate) {
        srcFrame = VideoFrameUtil::rotate(srcFrame, mRotate);
    }

    int dstWidth;
    int dstHeight;
    int cropWidth = 0;
    int cropHeight = 0;

    if (mOutConfig) {
        dstWidth = mOutConfig->getWidth();
        dstHeight = mOutConfig->getHeight();
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
    LOGI("convertFrame needScale:%d %d*%d", needScale, dstWidth, dstHeight)
    if (needScale) {
        srcFrame = VideoFrameUtil::scale(srcFrame, dstWidth, dstHeight);
    }
    bool needCrop = false;
    if (cropWidth != 0 && cropHeight != 0 &&
            (cropWidth < srcFrame->width || cropHeight < srcFrame->height)) {
        needCrop = true;
    }
    if (needCrop) {
        srcFrame = VideoFrameUtil::crop(srcFrame, cropWidth, cropHeight,
                                        (srcFrame->width - cropWidth) / 2,
                                        (srcFrame->height - cropHeight) / 2);
    }
    int result = av_frame_ref(dstFrame, srcFrame);

    LOGI("convertFrame done result:%d pts:%ld(%f) format:%s %d*%d", result,
         dstFrame->pts, dstFrame->pts * av_q2d(dstFrame->time_base),
         av_get_pix_fmt_name((AVPixelFormat) dstFrame->format),
         dstFrame->width, dstFrame->height)
    av_frame_free(&srcFrame);
    srcFrame = nullptr;
}

void VideoDecoder::resultCallback(AVFrame *srcFrame) {
    updateTimestamp(srcFrame);
    int dstWidth = srcFrame->width;
    int dstHeight = srcFrame->height;
    LOGI("resultCallback pts:%ld(%lf) format:%s %d*%d", srcFrame->pts,
         srcFrame->pts * av_q2d(srcFrame->time_base),
         av_get_pix_fmt_name((AVPixelFormat) srcFrame->format), dstWidth, dstHeight)
    // mAvFrame->format == AV_PIX_FMT_YUV420P10LE先转为RGBA进行渲染
    AVFrame *dstFrame = av_frame_alloc();
    dstFrame->format = AV_PIX_FMT_RGBA;
    dstFrame->width = dstWidth;
    dstFrame->height = dstHeight;
    av_frame_get_buffer(dstFrame, 0);
    dstFrame->pts = srcFrame->pts;
    dstFrame->pkt_duration = srcFrame->pkt_duration;
    dstFrame->time_base = srcFrame->time_base;
    if (convertToSurface(srcFrame, dstFrame) > 0) {
        LOGI("resultCallback convertToSurface done")
        if (mOnFrameArrivedListener) {
            mOnFrameArrivedListener(dstFrame);
        }
        LOGI("resultCallback convertToSurface end")
    } else {
        LOGE("showFrameToWindow convertToSurface fail")
    }
    LOGI("resultCallback frame:%p", &dstFrame)
    av_frame_free(&dstFrame);
    dstFrame = nullptr;
}

void VideoDecoder::showFrameToWindow(AVFrame *pFrame) {
    if (!nativeWindow) {
        LOGE("showFrameToWindow nativeWindow is null")
        return;
    }
    LOGI("showFrameToWindow pts:%ld(%lf) format:%s data:%p %d %d*%d", pFrame->pts,
         pFrame->pts * av_q2d(pFrame->time_base),
         av_get_pix_fmt_name((AVPixelFormat) pFrame->format), &pFrame->data[3],
         pFrame->linesize[0],
         pFrame->width, pFrame->height)
    auto startTime = std::chrono::steady_clock::now();
    //将yuv数据转成rgb

    //windowBuffer 入参出参,
    mSurfaceMutexObj->lock();
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
        if (!nativeWindow) {
            break;
        }
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
    if (nativeWindow) {
        result = ANativeWindow_unlockAndPost(nativeWindow);
    }
    LOGI("showFrameToWindow ANativeWindow_unlockAndPost %d %s", result, av_err2str(result))
    mSurfaceMutexObj->unlock();
}

int VideoDecoder::convertToSurface(AVFrame *srcFrame, AVFrame *dstFrame) {
    if (mSwsContext == nullptr) {
        LOGI("convertToSurface %d,%d,%s   %d,%d,%s", srcFrame->width, srcFrame->height,
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


void VideoDecoder::seekLock() {
    mSeekMutexObj->lock();
}

void VideoDecoder::seekUnlock() {
    mSeekMutexObj->unlock();
}

void VideoDecoder::updateTimestamp(AVFrame *frame) {
    if (mStartTimeMsForSync < 0) {
        LOGI("update video start time")
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
        LOGI("fix video start time")
    }
}

int VideoDecoder::getWidth() const {
    return mWidth;
}


int VideoDecoder::getHeight() const {
    return mHeight;
}

double VideoDecoder::getFps() const {
    return mFps;
}

double VideoDecoder::getConfigOutFps() const {
    if (mOutConfig) {
        return mOutConfig->getFps();
    }
    return 0;
}

double VideoDecoder::getTargetFps() const {
    double fps = getConfigOutFps();
    if (fps == 0) {
        fps = getFps();
    }
    return fps;
}

int VideoDecoder::getTargetWidth() {
    int width = getConfigCropWidth();
    if (width == 0) {
        width = getConfigWidth();
        if (width == 0) {
            width = getWidth();
        }
    }
    return width;
}

int VideoDecoder::getTargetHeight() {
    int width = getConfigCropHeight();
    if (width == 0) {
        width = getConfigHeight();
        if (width == 0) {
            width = getHeight();
        }
    }
    return width;
}

int VideoDecoder::getConfigWidth() {
    if (mOutConfig) {
        return mOutConfig->getWidth();
    }
    return 0;
}

int VideoDecoder::getConfigHeight() {
    if (mOutConfig) {
        return mOutConfig->getHeight();
    }
    return 0;
}

int VideoDecoder::getConfigCropWidth() {
    if (mOutConfig) {
        return mOutConfig->getCropWidth();
    }
    return 0;
}

int VideoDecoder::getConfigCropHeight() {
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
    LOGI("[video] seek to: %ld, seekPos: %" PRId64 ", ret: %d", pos, seekPos, ret)
    // seek后需要恢复起始时间
    mFixStartTime = true;
    releaseFilter();
    initFilter();
    return ret;
}

void VideoDecoder::release() {
    LOGI("VideoDecoder::release()")
    mFixStartTime = false;
    mStartTimeMsForSync = -1;

    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
        LOGI("mSwsContext context...release")
    }
    if (mTransformContext) {
        sws_freeContext(mTransformContext);
        mTransformContext = nullptr;
        LOGI("mTransformContext context...release")
    }
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
        mSurface = nullptr;
    }
    if (dstWindowBuffer) {
        free(dstWindowBuffer);
        dstWindowBuffer = nullptr;
    }

    if (mCodecContext) {
        avcodec_free_context(&mCodecContext);
        mCodecContext = nullptr;
        LOGI("codec...release")
    }
    if (mOutConfig) {
        mOutConfig = nullptr;
    }
    if (mSeekMutexObj) {
        mSeekMutexObj = nullptr;
    }
    if (mSurfaceMutexObj) {
        mSurfaceMutexObj = nullptr;
    }
    releaseFilter();
}