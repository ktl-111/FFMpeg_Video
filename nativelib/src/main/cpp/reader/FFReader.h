#ifndef FFMPEGDEMO_FFREADER_H
#define FFMPEGDEMO_FFREADER_H

#include <string>

extern "C" {
#include "../include/libavformat/avformat.h"
#include "../include/libavcodec/avcodec.h"
#include "../include/libswscale/swscale.h"
#include "../include/libavutil/imgutils.h"
}

enum TrackType {
    Track_Video,
    Track_Audio
};

enum DiscardType {
    DISCARD_NONE,   // discard nothing
    DISCARD_NONREF, // discard all non reference
    DISCARD_NONKEY  // discard all frames except keyframes
};

typedef struct MediaInfo {
    // video
    int width = -1;
    int height = -1;
    int videoIndex = -1;
    AVRational video_time_base;

    // audio
    int audioIndex = -1;
    AVRational audio_time_base;

} MediaInfo;

/**
 * read AVPacket class
 */
class FFReader {

public:
    FFReader();
    virtual ~FFReader();

    virtual bool init(std::string &path);

    bool selectTrack(TrackType type);

    int fetchAvPacket(AVPacket *pkt);

    bool isKeyFrame(AVPacket *pkt);

    /**
     * 获取timestamp对应的关键帧index，基于BACKWARD
     * @param timestamp: 时间单位s
     * @return
     */
    int getKeyFrameIndex(int64_t timestamp);

    double getDuration();

    /**
     * seek
     * @param timestamp: 时间单位s
     */
    void seek(int64_t timestamp);

    void flush();

    void setDiscardType(DiscardType type);

    AVCodecContext *getCodecContext();

    AVCodecParameters *getCodecParameters();

    MediaInfo getMediaInfo();

    void release();

protected:
    AVFormatContext *mFtx = nullptr;
    MediaInfo mMediaInfo;

private:
    const AVCodec *mCodecArr[2]{nullptr, nullptr};
    AVCodecContext *mCodecContextArr[2]{nullptr, nullptr};

    int mVideoIndex = -1;
    int mAudioIndex = -1;
    int mCurStreamIndex = -1;
    TrackType mCurTrackType = Track_Video;

    DiscardType mDiscardType = DISCARD_NONE;

    int prepare();
};


#endif //FFMPEGDEMO_FFREADER_H
