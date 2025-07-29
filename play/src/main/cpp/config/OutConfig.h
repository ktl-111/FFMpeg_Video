//
// Created by Administrator on 2023/12/16.
//

#ifndef VIDEOLEARN_OUTCONFIG_H
#define VIDEOLEARN_OUTCONFIG_H

extern "C" {
#include "libavutil/rational.h"
};

class OutConfig {
public:
    OutConfig(int width, int height, int cropWidth, int cropHeight, int fps);

    ~OutConfig();

    int getWidth();

    int getHeight();

    int getCropWidth();

    int getCropHeight();

    int getFps();

    AVRational getTimeBase();

private:
    int mWidth;
    int mHeight;
    int mCropWidth;
    int mCropHeight;
    int mFps;
    AVRational mTimeBase{};
};

#endif //VIDEOLEARN_OUTCONFIG_H
