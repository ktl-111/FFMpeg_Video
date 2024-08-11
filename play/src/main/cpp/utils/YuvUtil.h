//
// Created by 80263405 on 2023/12/19.
//

#ifndef VIDEOSDK_YUVUTIL_H
#define VIDEOSDK_YUVUTIL_H

#include <stdint.h>
#include "../libyuv/libyuv/scale.h"
#include "../libyuv/libyuv.h"

extern "C" {
#include "libswscale/swscale.h"
}

class YuvUtil {

public:

    /**
     * 缩放
     * @param src
     * @param width
     * @param height
     * @param dst
     * @param dst_width
     * @param dst_height
     * @param mode
     */
    static int scale(uint8_t *src, int width, int height, uint8_t *dst, int dst_width,
              int dst_height, int mode);



    static int scale(AVFrame *src, AVFrame *dst, int mode);

    /**
     * 旋转
     * @param src
     * @param width
     * @param height
     * @param dst
     * @param degree
     */
    static int rotate(uint8_t *src, int width, int height, uint8_t *dst, int degree);

    /**
     * 裁剪
     * @param src
     * @param width
     * @param height
     * @param dst
     * @param dst_width
     * @param dst_height
     * @param left
     * @param top
     */
    static int crop(uint8_t *src, int width, int height, uint8_t *dst, int dst_width, int dst_height,
             int left, int top);

//private:

};

#endif //VIDEOSDK_YUVUTIL_H
