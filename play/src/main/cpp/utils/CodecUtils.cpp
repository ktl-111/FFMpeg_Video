#include "CodecUtils.h"
#include "string"
#include "Logger.h"

const AVCodec *CodecUtils::findDecodec(AVCodecID codecId, bool useHw) {
    const AVCodec *mVideoCodec = nullptr;
    std::string mediacodecName;
    if (useHw) {
        switch (codecId) {
            case AV_CODEC_ID_H264:
                mediacodecName = "h264_mediacodec";
                break;
            case AV_CODEC_ID_HEVC:
                mediacodecName = "hevc_mediacodec";
                break;
            default:
                useHw = false;
                LOGE("findDecodec format(%d) not support hw decode, maybe rebuild ffmpeg so",
                     codecId)
                break;
        }
    }
    if (useHw) {
        enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
        AVHWDeviceType type = av_hwdevice_find_type_by_name("mediacodec");
        const AVCodec *mediacodec = avcodec_find_decoder_by_name(mediacodecName.c_str());
        if (mediacodec) {
            LOGE("findDecodec find %s", mediacodecName.c_str())
            for (int i = 0;; ++i) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(mediacodec, i);
                if (!config) {
                    LOGE("findDecodec Decoder: %s does not support device type: %s",
                         mediacodec->name,
                         av_hwdevice_get_type_name(type))
                    break;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                        config->device_type == type) {
                    // AV_PIX_FMT_MEDIACODEC(165)
                    hw_pix_fmt = config->pix_fmt;
                    LOGI("findDecodec Decoder: %s support device type: %s, hw_pix_fmt: %d, AV_PIX_FMT_MEDIACODEC: %d",
                         mediacodec->name, av_hwdevice_get_type_name(type), hw_pix_fmt,
                         AV_PIX_FMT_MEDIACODEC);
                    break;
                }
            }

            if (hw_pix_fmt != AV_PIX_FMT_NONE) {
                mVideoCodec = mediacodec;
            }
        }
    }
    if (mVideoCodec == nullptr) {
        mVideoCodec = avcodec_find_decoder(codecId);
    }
    if (mVideoCodec) {
        LOGI("findDecodec:%s", mVideoCodec->name)
    }
    return mVideoCodec;
}