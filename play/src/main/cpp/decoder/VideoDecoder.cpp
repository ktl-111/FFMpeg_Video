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
//

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

    // find decoder
    bool useHwDecoder = true;
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

    mVideoCodec = avcodec_find_decoder(params->codec_id);
    LOGI("find codec %s", mVideoCodec->name)

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

        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         getCropWidth() != 0 ? getCropWidth() : getWidth(),
                                         getCropHeight() != 0 ? getCropHeight() : getHeight(),
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

    return true;
}

void VideoDecoder::surfaceReCreate(JNIEnv *env, jobject surface) {
    LOGI("[video] surfaceReCreate")
    mSurface = surface;
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

void VideoDecoder::surfaceDestroy(JNIEnv *env) {
    LOGI("[video] surfaceDestroy")
    mSurface = nullptr;
    ANativeWindow_release(nativeWindow);
    nativeWindow = nullptr;
}

int VideoDecoder::converFrameTo420Frame(AVFrame *srcFrame, AVFrame *dstFrame) {
    if (mTransformContext == nullptr) {
        LOGI("converFrameTo420Frame %d,%d,%s   %d,%d,%s", srcFrame->width, srcFrame->height,
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
            "[video] avcodec_send_packet...pts: %" PRId64 "(%f), dts: %" PRId64 ", isKeyFrame: %d, res: %d, isEof: %d,extra_hw_frames:%d",
            avPacket->pts, avPacket->pts * av_q2d(getStream()->time_base) * 1000, avPacket->dts,
            isKeyFrame, sendRes, isEof,
            mCodecContext->extra_hw_frames)
    // avcodec_receive_frame的-11，表示需要发新帧
    receiveRes = avcodec_receive_frame(mCodecContext, pAvFrame);

    LOGI("[video] avcodec_receive_frame %d %s", receiveRes, av_err2str(receiveRes))

    if (receiveRes != 0) {
        LOGI("[video] avcodec_receive_frame err: %d, resent: %d", receiveRes, mNeedResent)
        av_frame_free(&pAvFrame);
        // force EOF
        if (isEof && receiveRes == AVERROR_EOF) {
            receiveRes = AVERROR_EOF;
            mNeedResent = false;
        }
        return receiveRes;
    }

    convertFrame(pAvFrame, frame);

    return receiveRes;
}

void VideoDecoder::convertFrame(AVFrame *srcFrame, AVFrame *dstFrame) {
    auto ptsMs = srcFrame->pts * av_q2d(getStream()->time_base) * 1000;
    LOGI("convertFrame avcodec_receive_frame...pts: %" PRId64 ", time: %f, format: %s, need retry: %d",
         srcFrame->pts, ptsMs, av_get_pix_fmt_name((AVPixelFormat) srcFrame->format),
         mNeedResent)

    bool isRotate = mRotate != 0;
    if (srcFrame->format != AV_PIX_FMT_YUV420P) {
        AVFrame *converSrcFrame = av_frame_alloc();
        converSrcFrame->format = AV_PIX_FMT_YUV420P;
        converSrcFrame->width = srcFrame->width;
        converSrcFrame->height = srcFrame->height;
        av_frame_get_buffer(converSrcFrame, 0);
        converSrcFrame->pts = srcFrame->pts;
        converSrcFrame->duration = srcFrame->duration;
        converSrcFrame->time_base = srcFrame->time_base;
        int ret = converFrameTo420Frame(srcFrame, converSrcFrame);
        LOGI("convertFrame converFrameTo420Frame ret:%d", ret)
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = converSrcFrame;
    }

    if (isRotate) {
        AVFrame *rotateFrame = av_frame_alloc();
        if (mRotate == 90 || mRotate == 270) {
            rotateFrame->width = srcFrame->height;
            rotateFrame->height = srcFrame->width;
        } else {
            rotateFrame->width = srcFrame->width;
            rotateFrame->height = srcFrame->height;
        }
        rotateFrame->pts = srcFrame->pts;
        rotateFrame->pkt_dts = srcFrame->pkt_dts;
        rotateFrame->duration = srcFrame->duration;
        rotateFrame->pkt_size = srcFrame->pkt_size;
        rotateFrame->format = srcFrame->format;
        int ret = av_frame_get_buffer(rotateFrame, 0);
        LOGI("convertFrame decode av_frame_get_buffer %d", ret)
        ret = libyuv::I420Rotate(srcFrame->data[0], srcFrame->linesize[0],
                                 srcFrame->data[1], srcFrame->linesize[1],
                                 srcFrame->data[2], srcFrame->linesize[2],
                                 rotateFrame->data[0], rotateFrame->linesize[0],
                                 rotateFrame->data[1], rotateFrame->linesize[1],
                                 rotateFrame->data[2], rotateFrame->linesize[2],
                                 srcFrame->width, srcFrame->height,
                                 libyuv::RotationMode(mRotate));
        LOGI("convertFrame I420Rotate %d %d*%d", ret, rotateFrame->width, rotateFrame->height)
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = rotateFrame;
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
        LOGI("convertFrame Scale:%d", ret)
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
        LOGI("convertFrame av_frame_get_buffer:%d src:%d*%d d:%d-%d crop:%d*%d", ret,
             srcWidth,
             srcHeight, dx, dy,
             cropWidth, cropHeight)

        int size = av_image_get_buffer_size((AVPixelFormat) srcFrame->format,
                                            srcFrame->width, srcFrame->height, 1);
        auto *buffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));

        ret = av_image_copy_to_buffer(buffer, size, srcFrame->data, srcFrame->linesize,
                                      (AVPixelFormat) srcFrame->format,
                                      srcFrame->width, srcFrame->height, 1);

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
        LOGI("convertFrame Crop:%d", ret)
        av_free(buffer);
        av_frame_free(&srcFrame);
        srcFrame = nullptr;
        srcFrame = cropFrame;
    }
    int result = av_frame_ref(dstFrame, srcFrame);
    dstFrame->time_base = mTimeBase;

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
    dstFrame->duration = srcFrame->duration;
    dstFrame->time_base = srcFrame->time_base;
    if (converToSurface(srcFrame, dstFrame) > 0) {
        LOGI("resultCallback converToSurface done")
        if (mOnFrameArrivedListener) {
            mOnFrameArrivedListener(dstFrame);
        }
        LOGI("resultCallback converToSurface end")
    } else {
        LOGE("showFrameToWindow converToSurface fail")
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
         av_get_pix_fmt_name((AVPixelFormat) pFrame->format), &pFrame->data[3], pFrame->linesize[0],
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

int VideoDecoder::converToSurface(AVFrame *srcFrame, AVFrame *dstFrame) {
    if (mSwsContext == nullptr) {
        LOGI("converToSurface %d,%d,%s   %d,%d,%s", srcFrame->width, srcFrame->height,
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
    return mFps;
}

double VideoDecoder::getOutFps() const {
    if (mOutConfig) {
        return mOutConfig->getFps();
    }
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
    LOGI("[video] seek to: %ld, seekPos: %" PRId64 ", ret: %d", pos, seekPos, ret)
    // seek后需要恢复起始时间
    mFixStartTime = true;
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
}