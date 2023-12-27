#ifndef FFMPEGDEMO_VIDEODECODER_H
#define FFMPEGDEMO_VIDEODECODER_H

#include <jni.h>
#include <functional>
#include <string>
#include <map>
#include <OutConfig.h>
#include "BaseDecoder.h"
#include "MutexObj.h"
#include <android/native_window_jni.h>

extern "C" {
#include "../include/libavcodec/mediacodec.h"
#include "../include/libswscale/swscale.h"
#include "../include/libavutil/imgutils.h"
#include "../include/libavutil/display.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

class VideoDecoder : public BaseDecoder {

public:
    VideoDecoder(int index, AVFormatContext *ftx);

    ~VideoDecoder();

    int getWidth() const;

    int getHeight() const;

    double getFps() const;

    void setSurface(jobject surface);

    void setOutConfig(const std::shared_ptr<OutConfig> outConfig);

    virtual double getDuration() override;

    virtual bool prepare(JNIEnv *env) override;

    virtual int decode(AVPacket *packet, AVFrame *frame) override;

    virtual void avSync(AVFrame *frame) override;

    virtual int seek(int64_t pos) override;

    virtual void release() override;

    int64_t getTimestamp() const;

    int getRotate();

    virtual void showFrameToWindow(AVFrame *frame);

    virtual void resultCallback(AVFrame *pFrame);

    void updateTimestamp(AVFrame *frame);

    void lock();

    void unlock();

private:
    int mWidth = -1;
    int mHeight = -1;
    double mFps = -1;
    uint8_t *shadowedOutbuffer;
    ANativeWindow *nativeWindow;
    ANativeWindow_Buffer windowBuffer;
    std::shared_ptr<MutexObj> mSeekMutexObj;

    int RETRY_RECEIVE_COUNT = 7;

    int64_t mStartTimeMsForSync = -1;
    int64_t mCurTimeStampMs = 0;

    jobject mSurface = nullptr;
    std::shared_ptr<OutConfig> mOutConfig = nullptr;
    AVFilterContext *mBuffersrcContext = nullptr;
    AVFilterContext *mBuffersinkContext = nullptr;
    AVBufferRef *mHwDeviceCtx = nullptr;

    const AVCodec *mVideoCodec = nullptr;

    AVMediaCodecContext *mMediaCodecContext = nullptr;

    SwsContext *mSwsContext = nullptr;

    int swsScale(AVFrame *srcFrame, AVFrame *dstFrame);

    bool isHwDecoder(AVFrame *frame);

    bool initFilter();
};


#endif //FFMPEGDEMO_VIDEODECODER_H
