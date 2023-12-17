#include "OutConfig.h"
#include "../globals.h"

OutConfig::OutConfig(int width, int height, double fps) {
    mWidth = width;
    mHeight = height;
    mFps = fps;
    double d = mFps * TimeBaseDiff;

    mTimeBase = {1, ((int) mFps) * TimeBaseDiff};
}

OutConfig::~OutConfig() {

}

int OutConfig::getWidth() {
    return mWidth;
}

int OutConfig::getHeight() {
    return mHeight;
}

double OutConfig::getFps() {
    return mFps;
}

AVRational OutConfig::getTimeBase() { return mTimeBase; }
