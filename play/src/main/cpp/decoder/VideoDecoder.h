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

    void initConfig(JNIEnv *env, jobject out_config);

    int getTargetWidth();

    int getTargetHeight();

    int getConfigWidth();

    int getConfigHeight();

    int getConfigCropWidth();

    int getConfigCropHeight();

    double getTargetFps() const;

    double getConfigOutFps() const;

    int getWidth() const;

    int getHeight() const;

    double getFps() const;

    void setSurface(jobject surface);

    void setOutConfig(const std::shared_ptr<OutConfig> outConfig);

    virtual double getDuration() override;

    virtual bool prepare(JNIEnv *env) override;

    virtual int decode(AVPacket *packet, AVFrame *frame) override;

    void convertFrame(AVFrame *srcFrame, AVFrame *dstFrame);

    virtual void avSync(AVFrame *frame) override;

    virtual int seek(int64_t pos) override;

    virtual void release() override;

    int64_t getTimestamp() const;

    virtual void showFrameToWindow(AVFrame *frame);

    virtual void resultCallback(AVFrame *pFrame);

    void updateTimestamp(AVFrame *frame);

    void seekLock();

    void seekUnlock();

    void surfaceReCreate(JNIEnv *env, jobject surface);

    void surfaceDestroy(JNIEnv *env);

private:
    int mWidth = -1;
    int mHeight = -1;
    double mFps = -1;
    int mRotate;
    ANativeWindow *nativeWindow = nullptr;
    ANativeWindow_Buffer windowBuffer;
    uint8_t *dstWindowBuffer = nullptr;
    std::shared_ptr<MutexObj> mSeekMutexObj = nullptr;
    std::shared_ptr<MutexObj> mSurfaceMutexObj = nullptr;
    SwsContext *mTransformContext = nullptr;
    SwsContext *mSwsContext = nullptr;
    int64_t mStartTimeMsForSync = -1;
    int64_t mCurTimeMs = 0;

    jobject mSurface = nullptr;
    std::shared_ptr<OutConfig> mOutConfig = nullptr;

    const AVCodec *mVideoCodec = nullptr;
    AVFilterGraph *filter_graph = nullptr;

    AVFilterInOut *outputs = nullptr;
    AVFilterInOut *inputs = nullptr;
    AVFilterContext *buffersinkContext = nullptr;
    AVFilterContext *buffersrcContext = nullptr;

    void initFilter();

    void releaseFilter();

    int convertToSurface(AVFrame *srcFrame, AVFrame *dstFrame);

    int convertFrameTo420Frame(AVFrame *srcFrame, AVFrame *dstFrame);
};


#endif //FFMPEGDEMO_VIDEODECODER_H
