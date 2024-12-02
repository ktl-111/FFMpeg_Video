#include "VideoFrameUtil.h"


AVFrame *VideoFrameUtil::rotate(AVFrame *srcFrame, int degree) {
    AVFrame *targetFrame = av_frame_alloc();
    if (degree == 90 || degree == 270) {
        targetFrame->width = srcFrame->height;
        targetFrame->height = srcFrame->width;
    } else {
        targetFrame->width = srcFrame->width;
        targetFrame->height = srcFrame->height;
    }
    targetFrame->pts = srcFrame->pts;
    targetFrame->pkt_dts = srcFrame->pkt_dts;
    targetFrame->duration = srcFrame->duration;
    targetFrame->pkt_size = srcFrame->pkt_size;
    targetFrame->format = srcFrame->format;
    targetFrame->time_base = srcFrame->time_base;
    int ret = av_frame_get_buffer(targetFrame, 0);
    LOGI("rotation av_frame_get_buffer %d", ret)
    ret = libyuv::I420Rotate(srcFrame->data[0], srcFrame->linesize[0],
                             srcFrame->data[1], srcFrame->linesize[1],
                             srcFrame->data[2], srcFrame->linesize[2],
                             targetFrame->data[0], targetFrame->linesize[0],
                             targetFrame->data[1], targetFrame->linesize[1],
                             targetFrame->data[2], targetFrame->linesize[2],
                             srcFrame->width, srcFrame->height, libyuv::RotationMode(degree));
    LOGI("rotation I420Rotate %d*%d ret = %d ", ret, targetFrame->width, targetFrame->height)
    av_frame_free(&srcFrame);
    srcFrame = nullptr;
    return targetFrame;
}

AVFrame *VideoFrameUtil::scale(AVFrame *srcFrame, int dstWidth, int dstHeight) {
    AVFrame *targetFrame = av_frame_alloc();
    targetFrame->width = dstWidth;
    targetFrame->height = dstHeight;
    targetFrame->format = srcFrame->format;
    LOGI("scale dstWidth:%d dstHeight:%d", dstWidth, dstHeight)

    targetFrame->pts = srcFrame->pts;
    targetFrame->pkt_dts = srcFrame->pkt_dts;
    targetFrame->duration = srcFrame->duration;
    targetFrame->pkt_size = srcFrame->pkt_size;
    targetFrame->time_base = srcFrame->time_base;
    int ret = av_frame_get_buffer(targetFrame, 0);
    LOGI("scale av_frame_get_buffer ret = %d", ret)
    if (srcFrame->format == AV_PIX_FMT_BGRA) {
        ret = libyuv::ARGBScale(srcFrame->data[0], srcFrame->linesize[0],
                                srcFrame->width, srcFrame->height,
                                targetFrame->data[0], targetFrame->linesize[0],
                                dstWidth, dstHeight, libyuv::kFilterNone);
    } else {
        ret = libyuv::I420Scale(srcFrame->data[0], srcFrame->linesize[0],
                                srcFrame->data[1], srcFrame->linesize[1],
                                srcFrame->data[2], srcFrame->linesize[2],
                                srcFrame->width, srcFrame->height,
                                targetFrame->data[0], targetFrame->linesize[0],
                                targetFrame->data[1], targetFrame->linesize[1],
                                targetFrame->data[2], targetFrame->linesize[2],
                                dstWidth, dstHeight, libyuv::kFilterNone);
    }

    LOGI("scale scale ret = %d", ret)
    av_frame_free(&srcFrame);
    srcFrame = nullptr;
    return targetFrame;
}

AVFrame *VideoFrameUtil::crop(AVFrame *srcFrame, int cropWidth, int cropHeight, int left, int top) {
    int srcWidth = srcFrame->width;
    int srcHeight = srcFrame->height;
    AVFrame *targetFrame = av_frame_alloc();
    targetFrame->width = cropWidth;
    targetFrame->height = cropHeight;
    targetFrame->format = srcFrame->format;
    LOGI("cropFrame needCrop cropWidth :%d cropHeight:%d", cropWidth, cropHeight)

    targetFrame->pts = srcFrame->pts;
    targetFrame->pkt_dts = srcFrame->pkt_dts;
    targetFrame->duration = srcFrame->duration;
    targetFrame->time_base = srcFrame->time_base;
    int ret = av_frame_get_buffer(targetFrame, 0);

    int dx = left + left % 2;
    int dy = top + top % 2;
    LOGI("cropFrame av_frame_get_buffer:%d src:%d*%d d:%d-%d crop:%d*%d", ret, srcWidth, srcHeight,
         dx, dy, cropWidth, cropHeight)

    int size = av_image_get_buffer_size((AVPixelFormat) srcFrame->format,
                                        srcFrame->width, srcFrame->height, 1);
    auto *buffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));
    av_image_copy_to_buffer(buffer, size, srcFrame->data, srcFrame->linesize,
                            (AVPixelFormat) srcFrame->format,
                            srcFrame->width, srcFrame->height, 1);


    if (srcFrame->format == AV_PIX_FMT_BGRA) {
        ret = libyuv::ConvertToARGB(buffer, size,
                                    targetFrame->data[0], targetFrame->linesize[0],
                                    dx, dy,
                                    srcWidth, srcHeight,
                                    cropWidth, cropHeight,
                                    libyuv::kRotate0, libyuv::FOURCC_ARGB
        );
    } else {
        ret = libyuv::ConvertToI420(buffer, size,
                                    targetFrame->data[0], targetFrame->linesize[0],
                                    targetFrame->data[1], targetFrame->linesize[1],
                                    targetFrame->data[2], targetFrame->linesize[2],
                                    dx, dy,
                                    srcWidth, srcHeight,
                                    cropWidth, cropHeight,
                                    libyuv::kRotate0, libyuv::FOURCC_I420
        );
    }

    LOGI("cropFrame crop:%d", ret)
    av_free(buffer);
    av_frame_free(&srcFrame);
    srcFrame = nullptr;
    return targetFrame;
}

