#include "OutConfig.h"
#include "../globals.h"

OutConfig::OutConfig(int width, int height, int cropWidth, int cropHeight, int fps) {
    mWidth = width;
    mHeight = height;
    mCropWidth = cropWidth;
    mCropHeight = cropHeight;
    mFps = fps;

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

int OutConfig::getCropWidth() {
    return mCropWidth;
}

int OutConfig::getCropHeight() {
    return mCropHeight;
}

int OutConfig::getFps() {
    return mFps;
}

AVRational OutConfig::getTimeBase() { return mTimeBase; }
