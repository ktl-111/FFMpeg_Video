#ifndef VIDEOSDK_CODECUTILS_H
#define VIDEOSDK_CODECUTILS_H

extern "C" {
#include "../include/libavutil/hwcontext.h"
#include "../include/libavcodec/avcodec.h"
}

class CodecUtils {
public:
    static const AVCodec *findDecodec(AVCodecID codecId, bool useHw);
};


#endif //VIDEOSDK_CODECUTILS_H
