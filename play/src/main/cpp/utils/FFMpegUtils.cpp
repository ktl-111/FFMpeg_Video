#include <jni.h>
#include <string>
#include <loghelper.h>
#include "../reader/FFVideoReader.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_utils_FFMpegUtils_getVideoFramesCore(JNIEnv *env, jobject thiz, jstring path,
                                                           jint width, jint height,
                                                           jboolean precise, jobject callback) {
    // path
    const char *c_path = env->GetStringUTFChars(path, nullptr);
    std::string s_path = c_path;
    jclass jclazz = env->GetObjectClass(thiz);

    //构造帧buffer
    jmethodID allocateFrame = env->GetMethodID(jclazz, "allocateFrame",
                                               "(II)Ljava/nio/ByteBuffer;");

    // callback
    jclazz = env->GetObjectClass(callback);

    jmethodID onStart = env->GetMethodID(jclazz, "onStart", "(D)[D");
    jmethodID onProgress = env->GetMethodID(jclazz, "onProgress", "(Ljava/nio/ByteBuffer;DIIII)Z");
    jmethodID onEnd = env->GetMethodID(jclazz, "onEnd", "()V");

    FFVideoReader *videoReader = new FFVideoReader();
    videoReader->setDiscardType(DISCARD_NONREF);
    LOGI("getVideoFramesCore path:%s", c_path)
    videoReader->init(s_path);


    int videoWidth = videoReader->getMediaInfo().width;
    int videoHeight = videoReader->getMediaInfo().height;
    if (width <= 0 && height <= 0) {
        width = videoWidth;
        height = videoHeight;
    } else if (width > 0 && height <= 0) { // scale base width
        width += width % 2;
        if (width > videoWidth) {
            width = videoWidth;
        }
        height = (jint) (1.0 * width * videoHeight / videoWidth);
        height += height % 2;
    } else if (width <= 0) { // scale base height
        height += height % 2;
        if (height > videoHeight) {
            height = videoHeight;
        }
        width = (jint) (1.0 * height * videoWidth / videoHeight);
    }
    LOGI("video size: %dx%d, get frame size: %dx%d", videoWidth, videoHeight, width, height)

    //获取pts数组
    jdoubleArray ptsArrays = (jdoubleArray) env->CallObjectMethod(callback, onStart,
                                                                  videoReader->getDuration());
    jdouble *ptsArr = env->GetDoubleArrayElements(ptsArrays, nullptr);

    int ptsSize = env->GetArrayLength(ptsArrays);
    int rotate = videoReader->getRotate();
    LOGI("timestamps size: %d rotate:%d", ptsSize, rotate);

    for (int i = 0; i < ptsSize; ++i) {
        jobject frameBuffer = env->CallObjectMethod(thiz, allocateFrame, width, height);
        uint8_t *buffer = (uint8_t *) env->GetDirectBufferAddress(frameBuffer);
        //设置buffer地址
        memset(buffer, 0, width * height * 4);
        int64_t pts = ptsArr[i];
        videoReader->getFrame(pts, width, height, buffer, precise);
        jboolean abort = env->CallBooleanMethod(callback, onProgress, frameBuffer, ptsArr[i], width,
                                                height, rotate, i);
        if (abort) {
            LOGE("onProgress abort");
            break;
        }

    }
    videoReader->release();
    delete videoReader;
    env->CallVoidMethod(callback, onEnd);
    if (ptsArr != nullptr) {
        env->ReleaseDoubleArrayElements(ptsArrays, ptsArr, 0);
    }
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_play_utils_FFMpegUtils_nativeCutting(JNIEnv *env, jobject thiz, jstring srcPath,
                                                      jstring desPath) {
    const char *c_srcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *c_desPath = env->GetStringUTFChars(desPath, nullptr);
    AVFormatContext *avFormatContext = avformat_alloc_context();
    avformat_open_input(&avFormatContext, c_srcPath, nullptr, nullptr);
    int videIndex = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVFormatContext *outFormatContext;
    //打开文件输入流
    int result;

    result = avformat_alloc_output_context2(&outFormatContext, nullptr, nullptr, c_desPath);
    if (result < 0) {
        LOGI("cutting avformat_alloc_output_context2 result:%d", result);
        return -1;
    }


    result = avio_open(&outFormatContext->pb, c_desPath, AVIO_FLAG_WRITE);
    if (result < 0) {
        LOGE("cutting avio_open result:%d", result);
        return -1;
    }
    AVStream *inStream = avFormatContext->streams[videIndex];
    AVStream *outStream = avformat_new_stream(outFormatContext, NULL);
//    outStream->avg_frame_rate = av_make_q(15, 1);
//    outStream->time_base = av_inv_q(outStream->avg_frame_rate);
//    videoContext->framerate = outStream->avg_frame_rate;

    outStream->time_base = inStream->time_base;
    LOGI("cutting timebase{%d,%d}", inStream->time_base.num, inStream->time_base.den)
    result = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    if (result < 0) {
        LOGE("cutting avcodec_parameters_copy result:%d", result);
        return -1;
    }
    //设置codec_tag=0,内部会找对应封装格式的tag
    outStream->codecpar->codec_tag = 0;
    result = avformat_write_header(outFormatContext, nullptr);
    if (result < 0) {
        LOGE("cutting avformat_write_header result:%d", result);
        return -1;
    }

    double startTime = 5.0;
    double endTime = startTime + 5.0;
    auto timeBase = av_q2d(inStream->time_base);
    int rnd = AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX;
    //s=pts*timebase
    auto startPts = startTime / timeBase;
    auto endPts = endTime / timeBase;
    LOGI("cutting inStream startPts:%f", startPts)

    result = avformat_seek_file(avFormatContext, videIndex, INT64_MIN, startPts, INT64_MAX,
                                AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
//    result = av_seek_frame(avFormatContext, videIndex, startPts,
//                           AVSEEK_FLAG_ANY);
    if (result < 0) {
        LOGE("cutting av_seek_frame result:%d", result)
        return -1;
    }
    int64_t t_pts = -1;
    int64_t t_dts = -1;
    for (;;) {
        auto avPacket = av_packet_alloc();

        result = av_read_frame(avFormatContext, avPacket);
        if (result != 0) {
            LOGI("cutting read done!!!");
            av_packet_unref(avPacket);
            break;
        }
        if (avPacket->stream_index != videIndex) {
            av_packet_unref(avPacket);
            continue;
        }
        LOGI("===========start cutting video=============");

        if (avPacket->pts > endPts) {
            LOGE("cutting pts>endPts pts:%f endPts:%f", avPacket->pts * timeBase,
                 endPts * timeBase)
            av_packet_unref(avPacket);
            break;
        }

        LOGI("cutting pts avPacket->pts:(%f) startPts(%f)",
             timeBase * avPacket->pts, timeBase * startPts);
        LOGI("cutting dts avPacket->dts:(%f) startPts(%f)",
             timeBase * avPacket->dts, timeBase * startPts);
        if (t_pts == -1) {
            t_pts = avPacket->pts;
        }
        if (t_dts == -1) {
            t_dts = avPacket->dts;
        }
        // 时间基转换
        avPacket->pts = av_rescale_q_rnd(avPacket->pts - t_pts, inStream->time_base,
                                         outStream->time_base,
                                         (AVRounding) rnd);

        avPacket->dts = av_rescale_q_rnd(avPacket->dts - t_dts, inStream->time_base,
                                         outStream->time_base,
                                         (AVRounding) rnd);

        avPacket->duration = av_rescale_q(avPacket->duration, inStream->time_base,
                                          outStream->time_base);
        avPacket->pos = -1;

        LOGI("cutting av_rescale_q_rnd pts:%f,dts:%f,fps:%f,duration:%f,packet flags:%d",
             timeBase * avPacket->pts, timeBase * avPacket->dts,
             av_q2d(outStream->avg_frame_rate), timeBase * avPacket->duration, avPacket->flags);

        if (avPacket->pts < avPacket->dts) {
            LOGE("cutting pts<dts")
            av_packet_unref(avPacket);
            //处理异常数据,pts必大于dts
            continue;
        }

        result = av_interleaved_write_frame(outFormatContext, avPacket);
        if (result < 0) {
            av_packet_unref(avPacket);
            LOGE("cutting av_interleaved_write_frame result:%d", result);
            break;
        }
        av_packet_unref(avPacket);
        LOGI("cutting %s done", "");
    }
    LOGI("cutting done!!!");
    av_write_trailer(outFormatContext);
    return 0;
}