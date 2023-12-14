#ifndef FFMPEGDEMO_VIDEODECODER_H
#define FFMPEGDEMO_VIDEODECODER_H

#include <jni.h>
#include <functional>
#include <string>
#include "BaseDecoder.h"
#include <android/native_window_jni.h>

extern "C" {
#include "../include/libavcodec/mediacodec.h"
#include "../include/libswscale/swscale.h"
#include "../include/libavutil/imgutils.h"
#include "../include/libavutil/display.h"
}

class VideoDecoder : public BaseDecoder {

public:
    VideoDecoder(int index, AVFormatContext *ftx);

    ~VideoDecoder();

    int getWidth() const;

    int getHeight() const;

    double getFps() const;

    void setSurface(jobject surface);

    virtual double getDuration() override;

    virtual bool prepare(JNIEnv *env) override;

    virtual int decode(AVPacket *packet,AVFrame *frame) override;

    virtual void avSync(AVFrame *frame) override;

    virtual int seek(double pos) override;

    virtual void release() override;

    int64_t getTimestamp() const;

    int getRotate();

    virtual void showFrameToWindow(AVFrame *frame);
    virtual void resultCallback(AVFrame *pFrame);

    void updateTimestamp(AVFrame *frame);
private:
    int mWidth = -1;
    int mHeight = -1;
    double mFps = -1;
    int scale = 1;
    uint8_t *shadowedOutbuffer;
    ANativeWindow *nativeWindow;
    ANativeWindow_Buffer windowBuffer;

    int RETRY_RECEIVE_COUNT = 7;

    int64_t mStartTimeMsForSync = -1;
    int64_t mCurTimeStampMs = 0;

    jobject mSurface = nullptr;

    AVBufferRef *mHwDeviceCtx = nullptr;

    const AVCodec *mVideoCodec = nullptr;

    AVMediaCodecContext *mMediaCodecContext = nullptr;

    SwsContext *mSwsContext = nullptr;

    int swsScale(AVFrame *srcFrame, AVFrame *swFrame);

    bool isHwDecoder();
};


#endif //FFMPEGDEMO_VIDEODECODER_H
