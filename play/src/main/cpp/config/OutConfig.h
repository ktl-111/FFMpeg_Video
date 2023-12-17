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
    OutConfig(int width, int height, double fps);

    ~OutConfig();

    int getWidth();

    int getHeight();

    double getFps();

    AVRational getTimeBase();

private:
    int mWidth;
    int mHeight;
    double mFps;
    AVRational mTimeBase{};
};

#endif //VIDEOLEARN_OUTCONFIG_H
