#include <assert.h>
#include "FFVideoReader.h"
#include "Logger.h"
#include "../utils/CommonUtils.h"
#include "../libyuv/libyuv.h"
#include "../globals.h"

extern "C" {
#include "../include/libavutil/display.h"
}

FFVideoReader::FFVideoReader() {
    LOGI("FFVideoReader")
}

FFVideoReader::~FFVideoReader() {
    LOGI("~FFVideoReader")
    if (mSwsContext != nullptr) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }

    mScaleBufferSize = -1;
    if (mScaleBuffer != nullptr) {
        free(mScaleBuffer);
        mScaleBuffer = nullptr;
    }

    if (mAvFrame != nullptr) {
        av_frame_free(&mAvFrame);
        av_free(mAvFrame);
        mAvFrame = nullptr;
    }
}

std::string FFVideoReader::init(std::string &path, bool useHw) {
    const std::string &init = FFReader::init(path, useHw);
    std::string result = init;
    if (isSuccess(result)) {
        result = selectTrack(Track_Video, useHw);
    }

    LOGI("[FFVideoReader], init: %s", result.c_str());
    return result;
}

void FFVideoReader::getFrame(int64_t pts, int width, int height, uint8_t *buffer, bool precise) {
    int64_t start = getCurrentTimeMs();
    LOGI(
            "[FFVideoReader], getFrame: %" PRId64 ", mLastPts: %" PRId64 ", width: %d, height: %d",
            pts, mLastPts, width, height)
    if (mLastPts == -1) {
        LOGI("[FFVideoReader], seek");
        seek(pts);
    } else if (!precise || getKeyFrameIndex(mLastPts) != getKeyFrameIndex(pts)) {
        LOGI("[FFVideoReader], flush & seek");
        flush();
        seek(pts);
    } else {
        // only need loop decode
        LOGI("[FFVideoReader], only need loop decode");
    }
    mLastPts = pts;

    AVCodecContext *codecContext = getCodecContext();
    MediaInfo mediaInfo = getMediaInfo();
    LOGI("[FFVideoReader], getFrame, origin: %dx%d, dst: %dx%d", mediaInfo.width,
         mediaInfo.height, width, height);
    bool needScale = mediaInfo.width != width || mediaInfo.height != height;

    AVFrame *frame = av_frame_alloc();
    AVPacket *pkt = av_packet_alloc();

    int64_t target = av_rescale_q(pts * AV_TIME_BASE, AV_TIME_BASE_Q, mediaInfo.video_time_base);
    bool find = false;
    int decodeCount = 0;
    while (true) {
        int ret = fetchAvPacket(pkt);
        if (ret < 0) {
            LOGE("[FFVideoReader], fetchAvPacket failed: %d", ret);
            break;
        }

        int sendRes = avcodec_send_packet(codecContext, pkt);
        int receiveRes = avcodec_receive_frame(codecContext, frame);
        decodeCount++;
        LOGD("[FFVideoReader], receiveRes: %d, sendRes: %d", receiveRes, sendRes);
        if (receiveRes == AVERROR(EAGAIN)) {
            continue;
        }

        // 非精准抽帧
        if (!precise) {
            find = true;
            break;
        }
        // 精准抽帧
        if (frame->pts >= target) {
            find = true;
            break;
        }

        av_packet_unref(pkt);
        av_frame_unref(frame);
    }

    if (find) {
        LOGI(
                "[FFVideoReader], get frame decode done, pts: %" PRId64 ", time: %f, format: %d, consume: %" PRId64 ", decodeCount: %d",
                frame->pts,
                (frame->pts * av_q2d(mediaInfo.video_time_base)),
                frame->format,
                (getCurrentTimeMs() - start), decodeCount);

        if (frame->format == AV_PIX_FMT_NONE) {
            assert(false);
        } else if (frame->format == AV_PIX_FMT_YUV420P) {
            if (needScale) {
                int64_t scaleBufferSize = width * height * 3 / 2;
                if (mScaleBuffer && scaleBufferSize != mScaleBufferSize) {
                    free(mScaleBuffer);
                    mScaleBuffer = nullptr;
                }
                mScaleBufferSize = scaleBufferSize;
                if (mScaleBuffer == nullptr) {
                    mScaleBuffer = (uint8_t *) malloc(scaleBufferSize);
                }

                auto scaleBuffer = mScaleBuffer;
                libyuv::I420Scale(
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2],
                        mediaInfo.width, mediaInfo.height,
                        scaleBuffer, width,
                        scaleBuffer + width * height, width / 2,
                        scaleBuffer + width * height * 5 / 4, width / 2,
                        width, height, libyuv::kFilterNone);

                libyuv::I420ToABGR(scaleBuffer, width,
                                   scaleBuffer + width * height, width / 2,
                                   scaleBuffer + width * height * 5 / 4, width / 2,
                                   buffer, width * 4, width, height);
            } else {
                libyuv::I420ToABGR(frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2],
                                   buffer, width * 4, width, height);
            }
        } else if (frame->format != AV_PIX_FMT_RGBA ||
                (frame->format == AV_PIX_FMT_RGBA && needScale)) {
            AVFrame *swFrame = av_frame_alloc();
            unsigned int size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
            auto *rgbaBuffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));
            av_image_fill_arrays(swFrame->data, swFrame->linesize, rgbaBuffer, AV_PIX_FMT_RGBA,
                                 width, height, 1);

            if (mSwsContext == nullptr) {
                mSwsContext = sws_getContext(frame->width, frame->height,
                                             AVPixelFormat(frame->format),
                                             width, height, AV_PIX_FMT_RGBA,
                                             SWS_BICUBIC, nullptr, nullptr, nullptr);
                if (!mSwsContext) {
                    LOGE("[FFVideoReader], sws_getContext failed")
                }
            }

            // transform
            int ret = sws_scale(mSwsContext,
                                reinterpret_cast<const uint8_t *const *>(frame->data),
                                frame->linesize,
                                0,
                                frame->height,
                                swFrame->data,
                                swFrame->linesize
            );
            if (ret <= 0) {
                LOGE("[FFVideoReader], sws_scale failed, ret: %d", ret);
            }

            uint8_t *src = swFrame->data[0];
            int srcStride = swFrame->linesize[0];
            int dstStride = width * 4;
            for (int i = 0; i < height; i++) {
                memcpy(buffer + i * dstStride, src + i * srcStride, srcStride);
            }

            av_frame_free(&swFrame);
            av_freep(&swFrame);
            av_free(rgbaBuffer);
        }
    }

    av_packet_unref(pkt);
    av_packet_free(&pkt);
    av_free(pkt);

    av_frame_unref(frame);
    av_frame_free(&frame);
    av_free(frame);
}


void FFVideoReader::getNextFrame(
        const std::function<void(int sendPacketCount, double costMilli,
                                 AVFrame *, std::string errorMsg)> &frameArrivedCallback) {
    if (mAvFrame == nullptr) {
        mAvFrame = av_frame_alloc();
    }
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = nullptr;
    int sendPacketCount = 0;
    int maxRetryCount = 20;
    auto startTime = std::chrono::steady_clock::now();
    std::string errorMsg = "";
    while (true) {
        int ret = fetchAvPacket(pkt);
        if (ret < 0) {
            LOGE("[FFVideoReader], getNextFrame fetchAvPacket failed: %d,%s", ret,
                 av_err2str(ret))
            errorMsg = av_err2str(ret);
            break;
        }

        int sendRes = avcodec_send_packet(getCodecContext(), pkt);
        int receiveRes = avcodec_receive_frame(getCodecContext(), mAvFrame);
        LOGD("[FFVideoReader], getNextFrame receiveRes: %d(%s), sendRes: %d(%s), isKeyFrame: %d",
             receiveRes, av_err2str(receiveRes), sendRes, av_err2str(sendRes), isKeyFrame(pkt));
        av_packet_unref(pkt);

        sendPacketCount++;
        if (receiveRes == AVERROR(EAGAIN)) {
            if (sendPacketCount >= maxRetryCount) {
                break;
            }
            continue;
        }

        if (receiveRes == 0) {
            frame = mAvFrame;
            errorMsg = RESULT_SUCCESS;
        } else {
            errorMsg = av_err2str(receiveRes);
        }
        break;
    }
    av_packet_free(&pkt);
    auto endTime = std::chrono::steady_clock::now();
    auto costMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    if (frameArrivedCallback) {
        frameArrivedCallback(sendPacketCount, costMilli, frame, errorMsg);
    }
    av_frame_unref(mAvFrame);
}

int FFVideoReader::getRotate(AVStream *stream) {
    AVDictionaryEntry *tag = nullptr;

    while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        LOGI("[video] metadata: %s, %s", tag->key, tag->value)
    }

    tag = av_dict_get(stream->metadata, "rotate", nullptr, 0);
    const char *string = tag == nullptr ? "-1" : tag->value;
    LOGI("try getRotate from tag(rotate): %s", string)
    int rotate;
    if (tag != nullptr) {
        rotate = atoi(tag->value);
    } else {
        uint8_t *displayMatrix = av_stream_get_side_data(stream, AV_PKT_DATA_DISPLAYMATRIX,
                                                         nullptr);
        double theta = 0;
        if (displayMatrix) {
            theta = -av_display_rotation_get((int32_t *) displayMatrix);
        }
        rotate = (int) theta;
    }

    LOGI("getRotate: %d", rotate);
    if (rotate < 0) { // CCW -> CC(Clockwise)
        rotate %= 360;
        rotate += 360;
        LOGI("getRotate fix: %d", rotate);
    }

    return rotate < 0 ? 0 : rotate;
}

int FFVideoReader::getRotate() {
    return getRotate(mFtx->streams[mMediaInfo.videoIndex]);
}

int FFVideoReader::getFps() {
    return av_q2d(mFtx->streams[mMediaInfo.videoIndex]->avg_frame_rate);
}
