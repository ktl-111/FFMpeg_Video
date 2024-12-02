#ifndef VIDEOSDK_VIDEO_FRAME_UTIL_H
#define VIDEOSDK_VIDEO_FRAME_UTIL_H


#include <Logger.h>
#include "libyuv/rotate.h"
#include "libyuv/scale.h"
#include "libyuv/convert.h"
#include "libyuv/video_common.h"
#include "libyuv/scale_argb.h"

extern "C" {
/*#include "libswscale/swscale.h"*/
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
}

class VideoFrameUtil {

public:

    /**
     * 视频帧旋转
     * @param srcFrame 需要处理的原视频帧
     * @param degree 角度
     */
    static AVFrame * rotate(AVFrame *srcFrame, int degree);

    /**
     * 视频帧缩放
     * @param srcFrame 需要处理的原视频帧
     * @param dstWidth 缩放的目标帧的宽
     * @param dstHeight 缩放的目标帧的高
     */
    static AVFrame * scale(AVFrame *srcFrame, int dstWidth, int dstHeight);

    /**
     * 频帧裁切
     * @param srcFrame  需要处理的原视频帧
     * @param cropWidth  裁切宽
     * @param cropHeight 裁切高
     * @param left 裁切x轴间距
     * @param top  裁切y轴间距
     */
    static AVFrame * crop(AVFrame *srcFrame, int cropWidth, int cropHeight, int left, int top);


};

#endif //VIDEOSDK_VIDEO_FRAME_UTIL_H
