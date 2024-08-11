//
// Created by 80263405 on 2023/12/19.
//
#include "YuvUtil.h"


int
YuvUtil::scale(uint8_t *src, int width, int height, uint8_t *dst, int dst_width, int dst_height,
               int mode) {
    int src_i420_y_size = width * height;
    int src_i420_u_size = (width >> 1) * (height >> 1);
    uint8_t *src_i420_y_data = src;
    uint8_t *src_i420_u_data = src + src_i420_y_size;
    uint8_t *src_i420_v_data = src + src_i420_y_size + src_i420_u_size;

    int dst_i420_y_size = dst_width * dst_height;
    int dst_i420_u_size = (dst_width >> 1) * (dst_height >> 1);
    uint8_t *dst_i420_y_data = dst;
    uint8_t *dst_i420_u_data = dst + dst_i420_y_size;
    uint8_t *dst_i420_v_data = dst + dst_i420_y_size + dst_i420_u_size;

    return libyuv::I420Scale((const uint8_t *) src_i420_y_data, width,
                             (const uint8_t *) src_i420_u_data, width >> 1,
                             (const uint8_t *) src_i420_v_data, width >> 1,
                             width, height,
                             (uint8_t *) dst_i420_y_data, dst_width,
                             (uint8_t *) dst_i420_u_data, dst_width >> 1,
                             (uint8_t *) dst_i420_v_data, dst_width >> 1,
                             dst_width, dst_height,
                             (libyuv::FilterMode) mode);
}

int YuvUtil::scale(AVFrame *src, AVFrame *dst, int mode) {
    return libyuv::I420Scale(
            src->data[0], src->linesize[0],
            src->data[1], src->linesize[1],
            src->data[2], src->linesize[2],
            src->width, src->height,
            dst->data[0], dst->linesize[0],
            dst->data[1], dst->linesize[1],
            dst->data[2], dst->linesize[2],
            dst->width, dst->height, (libyuv::FilterMode) mode);
}

int YuvUtil::rotate(uint8_t *src, int width, int height, uint8_t *dst, int degree) {
    int src_i420_y_size = width * height;
    int src_i420_u_size = (width >> 1) * (height >> 1);

    uint8_t *src_i420_y_data = src;
    uint8_t *src_i420_u_data = src + src_i420_y_size;
    uint8_t *src_i420_v_data = src + src_i420_y_size + src_i420_u_size;

    uint8_t *dst_i420_y_data = dst;
    uint8_t *dst_i420_u_data = dst + src_i420_y_size;
    uint8_t *dst_i420_v_data = dst + src_i420_y_size + src_i420_u_size;

    //要注意这里的width和height在旋转之后是相反的
    if (degree == libyuv::kRotate90 || degree == libyuv::kRotate270) {
        return libyuv::I420Rotate((const uint8_t *) src_i420_y_data, width,
                                  (const uint8_t *) src_i420_u_data, width >> 1,
                                  (const uint8_t *) src_i420_v_data, width >> 1,
                                  (uint8_t *) dst_i420_y_data, height,
                                  (uint8_t *) dst_i420_u_data, height >> 1,
                                  (uint8_t *) dst_i420_v_data, height >> 1,
                                  width, height,
                                  (libyuv::RotationMode) degree);
    }

    return -1;
}

int
YuvUtil::crop(uint8_t *src, int width, int height, uint8_t *dst, int dst_width, int dst_height,
              int left, int top) {
    //裁剪的区域大小不对
    if (left + dst_width > (width + 1) || top + dst_height > (height + 1)) {
        return -1;
    }

    //left和top必须为偶数，否则显示会有问题
    if (left % 2 != 0 || top % 2 != 0) {
        return -1;
    }

    //默认处理YUV420P
    int src_size = width * height * 3 / 2;

    int dst_i420_y_size = dst_width * dst_height;
    int dst_i420_u_size = (dst_width >> 1) * (dst_height >> 1);

    uint8_t *dst_i420_y_data = dst;
    uint8_t *dst_i420_u_data = dst + dst_i420_y_size;
    uint8_t *dst_i420_v_data = dst + dst_i420_y_size + dst_i420_u_size;

    return libyuv::ConvertToI420((const uint8_t *) src, src_size,
                                 (uint8_t *) dst_i420_y_data, dst_width,
                                 (uint8_t *) dst_i420_u_data, dst_width >> 1,
                                 (uint8_t *) dst_i420_v_data, dst_width >> 1,
                                 left, top,
                                 width, height,
                                 dst_width, dst_height,
                                 libyuv::kRotate0, libyuv::FOURCC_I420);
}

