#include <jni.h>
#include <string>
#include <Logger.h>
#include <bits/sysconf.h>
#include "../reader/FFVideoReader.h"
#include "libyuv/scale.h"
#include "libyuv/convert.h"
#include "../globals.h"
#include "YuvUtil.h"
#include "libyuv/video_common.h"
#include "VideoDecoder.h"
#include "VideoFrameUtil.h"
#include "CodecUtils.h"

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/display.h"
#include "libavutil/opt.h"
}

AVFormatContext *inFormatContext = nullptr;
AVCodecContext *encodeContext = nullptr;
AVRational inTimeBase;
AVRational outTimeBase;
AVFilterContext *buffersinkContext = nullptr;
AVFilterContext *buffersrcContext = nullptr;
jobject callback = nullptr;
jmethodID onStart = nullptr;
jmethodID onProgress = nullptr;
jmethodID onFail = nullptr;
jmethodID onDone = nullptr;
double start_time = 0;
double end_time = 0;
AVStream *outStream = nullptr;
AVFormatContext *outFormatContext = nullptr;
std::shared_ptr<VideoDecoder> videoDecoder = nullptr;
AVFilterGraph *filter_graph = nullptr;
AVFilterInOut *outputs = nullptr;
AVFilterInOut *inputs = nullptr;

double progress = 0;
int writeFramsCount = 0;
jdouble frameCount = 0;
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_utils_DecodeUtils_nativeCutting(JNIEnv *env, jobject thiz,
                                                      jstring src_path, jstring dest_path,
                                                      jlong starttime,
                                                      jlong endtime,
                                                      jobject out_config,
                                                      jobject cb) {
    start_time = starttime / 1000.0;
    end_time = endtime / 1000.0;
    callback = cb;
    progress = 0;
    writeFramsCount = 0;
    // callback
    jclass jcalss = env->GetObjectClass(callback);
    onStart = env->GetMethodID(jcalss, "onStart", "()V");
    onProgress = env->GetMethodID(jcalss, "onProgress", "(D)V");
    onFail = env->GetMethodID(jcalss, "onFail", "(I)V");
    onDone = env->GetMethodID(jcalss, "onDone", "()V");
    env->CallVoidMethod(callback, onStart);
    const char *c_srcPath = env->GetStringUTFChars(src_path, nullptr);
    const char *c_desPath = env->GetStringUTFChars(dest_path, nullptr);

    inFormatContext = avformat_alloc_context();
    int result = 0;
    result = avformat_open_input(&inFormatContext, c_srcPath, NULL, NULL);
    if (result != 0) {
        LOGE("cutting not open input file,%d %s", result, av_err2str(result));
        env->CallVoidMethod(callback, onFail, ERRORCODE_OPENINPUT);
        return;
    }

    if (avformat_find_stream_info(inFormatContext, NULL) < 0) {
        LOGE("cutting avformat_find_stream_info fail");
        env->CallVoidMethod(callback, onFail, ERRORCODE_FIND_STERAM);
        return;
    }

    int inStreamIndex = av_find_best_stream(inFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1,
                                            nullptr, 0);
    if (inStreamIndex == AVERROR_STREAM_NOT_FOUND) {
        LOGI("cutting not find video index,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_NOT_FIND_VIDEO_STERAM);
        return;
    }

    videoDecoder = std::make_shared<VideoDecoder>(inStreamIndex,
                                                  inFormatContext);
    videoDecoder->initConfig(env, out_config);

    int outFps = (int) videoDecoder->getConfigOutFps();
    double srcFps = videoDecoder->getFps();
    if (outFps == 0) {
        outFps = (int) srcFps;
    } else if (srcFps < outFps) {
        LOGI("cutting src srcFps:(%f) < outFps:(%d)", srcFps, outFps)
        outFps = (int) srcFps;
    }
    bool prepareResult = videoDecoder->prepare(env);
    if (!prepareResult) {
        env->CallVoidMethod(callback, onFail, ERRORCODE_PREPARE_FILE);
        return;
    }
    AVStream *inSteram = videoDecoder->getStream();
    inTimeBase = inSteram->time_base;
    AVCodecParameters *codec_params = inSteram->codecpar;

    outTimeBase = {1, outFps * TimeBaseDiff};
    LOGI("cutting config,start_time:%lf end_time:%lf fps:%d timeBase:{%d,%d}", start_time, end_time,
         outFps, outTimeBase.num, outTimeBase.den)

    //设置filter
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    filter_graph = avfilter_graph_alloc();

    if (!buffersrc || !buffersink || !filter_graph) {
        LOGE("cutting buffersrc  buffersink avfilter_graph_alloc fail")
        env->CallVoidMethod(callback, onFail, ERRORCODE_GET_AVFILTER);
        return;
    }
    char args[512];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    AVCodecContext *decodecContext = videoDecoder->getCodecContext();
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             decodecContext->width, decodecContext->height, decodecContext->pix_fmt,
             inTimeBase.num, inTimeBase.den,
             decodecContext->sample_aspect_ratio.num, decodecContext->sample_aspect_ratio.den);
    LOGI("cutting args %s", args)
    result = avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in",
                                          args, NULL, filter_graph);

    if (result != 0) {
        LOGE("cutting buffersrc fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_CREATE_FILTER);
        return;
    }

    result = avfilter_graph_create_filter(&buffersinkContext, buffersink, "out",
                                          NULL, NULL, filter_graph);
    if (result != 0) {
        LOGE("cutting buffersink fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_CREATE_FILTER);
        return;
    }
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    //in->fps filter的输入端口名称
    //outputs->源头的输出
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrcContext;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    //out->fps filter的输出端口名称
    //outputs->filter后的输入
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersinkContext;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    const char *fpsTag = "fps=";
    char *fpsFilter = new char[strlen(fpsTag) + sizeof(outFps)];
    sprintf(fpsFilter, "%s%d", fpsTag, outFps);
    LOGI("cutting fpsfilter:%s", fpsFilter)
    //buffer->输出(outputs)->filter in(av_strdup("in")->fps filter->filter out(av_strdup("out"))->输入(inputs)->buffersink
    result = avfilter_graph_parse_ptr(filter_graph, fpsFilter, &inputs, &outputs, NULL);
    if (result != 0) {
        LOGE("cutting avfilter_graph_parse_ptr fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_FILTER_ERROR);
        return;
    }
    result = avfilter_graph_config(filter_graph, NULL);
    if (result != 0) {
        LOGE("cutting avfilter_graph_config fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_FILTER_ERROR);
        return;
    }


    outFormatContext = avformat_alloc_context();
    result = avformat_alloc_output_context2(&outFormatContext, NULL, NULL, c_desPath);
    if (result != 0) {
        LOGE("cutting avformat_alloc_output_context2 fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_ALLOC_OUTPUT_CONTEXT);
        return;
    }

    outStream = avformat_new_stream(outFormatContext, NULL);
    if (!outStream) {
        LOGE("cutting avformat_new_stream fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_COMMON);
        return;
    }
    if (avcodec_parameters_copy(outStream->codecpar, codec_params) < 0) {
        LOGE("cutting avcodec_parameters_copy fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_COMMON);
        return;
    }
    //设置对应参数
    outStream->codecpar->codec_id = AV_CODEC_ID_H264;
    outStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    outStream->codecpar->codec_tag = 0;
    outStream->avg_frame_rate = {outFps, 1};
    outStream->time_base = outTimeBase;
    outStream->start_time = 0;
    outStream->codecpar->width = videoDecoder->getTargetWidth();
    outStream->codecpar->height = videoDecoder->getTargetHeight();
    //    av_dict_copy(&outStream->metadata, inSteram->metadata, 0);
    //    outStream->side_data = inSteram->side_data;
    //    outStream->nb_side_data = inSteram->nb_side_data;
    //查找解码器
    const AVCodec *encoder = avcodec_find_encoder(outStream->codecpar->codec_id);
    if (!encoder) {
        LOGE("not find encoder")
        env->CallVoidMethod(callback, onFail, ERRORCODE_NOT_FIND_ENCODE);
        return;
    }
    LOGI("find encoder %s", encoder->name)
    encodeContext = avcodec_alloc_context3(encoder);
    encodeContext->codec_type = AVMEDIA_TYPE_VIDEO;
    encodeContext->width = outStream->codecpar->width;
    encodeContext->height = outStream->codecpar->height;
    encodeContext->time_base = outStream->time_base;
    encodeContext->framerate = outStream->avg_frame_rate;
    if (encoder->pix_fmts) {
        //设置解码器支持的第一个
        encodeContext->pix_fmt = encoder->pix_fmts[0];
    } else {
        encodeContext->pix_fmt = decodecContext->pix_fmt;
    }
    //    encodeContext->sample_aspect_ratio = decodecContext->sample_aspect_ratio;
    encodeContext->max_b_frames = 0;//不需要B帧
    encodeContext->gop_size = outStream->avg_frame_rate.num;//多少帧一个I帧

    encodeContext->qmax = 35;
    encodeContext->qmin = 10;
    encodeContext->bit_rate = 300 * 1000;

    //x264 ,设置编译速度,ultrafast,superfast,veryfast
    result = av_opt_set(encodeContext->priv_data, "preset", "veryfast", 0);
    if (result != 0) {
        LOGI("cutting preset set fail,%d %s", result, av_err2str(result))
    }

    //openh264,使用比特率模式
    result = av_opt_set(encodeContext->priv_data, "rc_mode", "bitrate", 0);
    if (result != 0) {
        LOGI("cutting rc_mode set fail,%d %s", result, av_err2str(result))
    }
    result = av_opt_set(encodeContext->priv_data, "profile", "high", 0);
    if (result != 0) {
        LOGI("cutting profile set fail,%d %s", result, av_err2str(result))
    }

    result = avcodec_open2(encodeContext, encoder, NULL);
    if (result != 0) {
        LOGI("cutting avcodec_open2 fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_AVCODEC_OPEN2);
        return;
    }

    result = avcodec_parameters_from_context(outStream->codecpar, encodeContext);
    if (result < 0) {
        LOGE("cutting avcodec_parameters_from_context fail,%d %s", result, av_err2str(result))
        env->CallVoidMethod(callback, onFail, ERRORCODE_PARAMETERS_TO_CONTEXT);
        return;
    }
    LOGI("cutting name:%s", avcodec_get_name(outFormatContext->video_codec_id))
    // 打开输出文件
    if (avio_open(&outFormatContext->pb, c_desPath, AVIO_FLAG_WRITE) < 0 ||
            avformat_write_header(outFormatContext, NULL) < 0) {
        LOGE("cutting not open des");
        env->CallVoidMethod(callback, onFail, ERRORCODE_OPEN_FILE);
        return;
    }

    LOGI("cutting seek %f", start_time / av_q2d(inTimeBase))
    if (start_time > 0) {
        avformat_seek_file(inFormatContext, inStreamIndex, INT64_MIN,
                           start_time / av_q2d(inTimeBase), INT64_MAX, 0);
    }

    frameCount = outFps * (end_time - start_time);
    progress += 1;
    env->CallVoidMethod(callback, onProgress, progress);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_play_utils_DecodeUtils_nativeWriteData(JNIEnv *env, jobject thiz, jobject buffer,
                                                        jlong time) {
    jint result = 0;
    while (1) {
        // 初始化filter图的帧
        AVPixelFormat srcFormat = AV_PIX_FMT_RGBA;
        uint8_t *pixels = (uint8_t *) env->GetDirectBufferAddress(buffer);


        AVRational outTimeBase = {1, (int) videoDecoder->getTargetFps() * TimeBaseDiff};
        AVFrame *frame = av_frame_alloc();
        frame->width = outStream->codecpar->width;
        frame->height = outStream->codecpar->height;
        frame->pts = time / av_q2d(outTimeBase) / 1000.0;
        frame->pkt_dts = frame->pts;
        frame->format = AV_PIX_FMT_YUV420P;
        frame->time_base = outTimeBase;
        frame->duration = TimeBaseDiff;
        result = av_frame_get_buffer(frame, 32);
        LOGI("cutting write data start result:%d size:%d*%d", result, frame->width, frame->height)
        result = libyuv::ABGRToI420(pixels, frame->width * 4,
                                    frame->data[0], frame->linesize[0],
                                    frame->data[1], frame->linesize[1],
                                    frame->data[2], frame->linesize[2],
                                    frame->width, frame->height);
//        result = sws_scale(swsGetContext,
//                           reinterpret_cast<const uint8_t *const *>(srcSlice),
//                           srcStride,
//                           0,
//                           videoDecoder->getHeight(),
//                           frame->data,
//                           frame->linesize);


        int sendResult = -1;
        int receiveResult = -1;
        LOGI("cutting frame pts:%ld(%f) dts:%ld(%f) duration:%ld(%f) flags:%d result:%d(%s) %d-%d",
             frame->pts,
             frame->pts * av_q2d(outTimeBase),
             frame->pkt_dts, frame->pkt_dts * av_q2d(outTimeBase),
             frame->duration, frame->duration * av_q2d(outTimeBase),
             frame->flags, result, av_err2str(result), inTimeBase.num, inTimeBase.den)


        if (frame->pts * av_q2d(outTimeBase) > end_time) {
            LOGI("cutting pts > endpts")
            av_frame_free(&frame);
            result = 10000;
            break;
        }
        AVFrame *dstFrame = av_frame_alloc();
        videoDecoder->convertFrame(frame, dstFrame);
        LOGI("cutting start encode pts:%ld(%f) %d %p", dstFrame->pts,
             dstFrame->pts * av_q2d(outTimeBase), dstFrame->linesize[0], &dstFrame->data)

        dstFrame->pict_type = AV_PICTURE_TYPE_NONE;
        do {
            sendResult = -1;
            receiveResult = -1;
            LOGI("cutting start avcodec_send_frame")
            sendResult = avcodec_send_frame(encodeContext, dstFrame);
            LOGI("cutting end avcodec_send_frame %d", sendResult)
            if (sendResult == 0) {
                AVPacket *receivePacket = av_packet_alloc();
                LOGI("cutting start avcodec_receive_packet")
                receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
                LOGI("cutting end avcodec_receive_packet %d", sendResult)
                if (receiveResult == 0) {
                    receivePacket->stream_index = outStream->index;
                    receivePacket->pts = writeFramsCount;
                    receivePacket->dts = receivePacket->pts - TimeBaseDiff;
                    receivePacket->duration = TimeBaseDiff;

                    LOGI("cutting av_interleaved_write_frame result pts:%ld %f dts:%ld %f duration:%ld %f",
                         receivePacket->pts,
                         receivePacket->pts * av_q2d(encodeContext->time_base),
                         receivePacket->dts,
                         receivePacket->dts * av_q2d(encodeContext->time_base),
                         receivePacket->duration,
                         receivePacket->duration * av_q2d(encodeContext->time_base))

                    result = av_write_frame(outFormatContext,
                                            receivePacket);
                    if (result == 0) {
                        writeFramsCount += TimeBaseDiff;
                        LOGI("cutting progress:%f %d %f",
                             writeFramsCount / TimeBaseDiff / frameCount,
                             writeFramsCount / TimeBaseDiff, frameCount)
                        progress = std::min(
                                writeFramsCount / TimeBaseDiff / frameCount * 100 + 1,
                                99.0);
//                        env->CallVoidMethod(callback, onProgress, progress);
                    } else {
                        LOGI("cutting av_interleaved_write_frame fail result %d %s", result,
                             av_err2str(result))
                    }
                }

                av_packet_free(&receivePacket);
            }
            LOGI("cutting encode sendResult:%d receiveResult:%d", sendResult, receiveResult)
        } while (sendResult == AVERROR(EAGAIN) && receiveResult == AVERROR(EAGAIN));
        // 清理资源
//        av_frame_free(&frame);
        av_frame_free(&dstFrame);
        LOGI("cutting av_write_frame")
        result = 0;
        break;
    }
    return result;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_play_utils_DecodeUtils_nativeEndDecode(JNIEnv *env, jobject thiz) {


    int sendResult = -1;
    int receiveResult = -1;
    int result = 0;
    do {
        sendResult = -1;
        receiveResult = -1;
        AVPacket *receivePacket = av_packet_alloc();
        sendResult = avcodec_send_frame(encodeContext, nullptr);
        if (sendResult == 0 || sendResult == AVERROR_EOF) {
            receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
            if (receiveResult == 0) {
                receivePacket->stream_index = outStream->index;
                receivePacket->pts = writeFramsCount;
                receivePacket->dts = receivePacket->pts;
                receivePacket->duration = TimeBaseDiff;

                LOGI("cutting av_interleaved_write_frame result flush pts:%ld %f dts:%ld %f duration:%ld %f",
                     receivePacket->pts,
                     receivePacket->pts * av_q2d(encodeContext->time_base),
                     receivePacket->dts,
                     receivePacket->dts * av_q2d(encodeContext->time_base),
                     receivePacket->duration,
                     receivePacket->duration * av_q2d(encodeContext->time_base))

                result = av_write_frame(outFormatContext,
                                        receivePacket);
                if (result == 0) {
                    writeFramsCount += TimeBaseDiff;
                    LOGI("cutting progress:%f %d %f",
                         writeFramsCount / TimeBaseDiff / frameCount,
                         writeFramsCount / TimeBaseDiff, frameCount)
                    progress = std::min(
                            writeFramsCount / TimeBaseDiff / frameCount * 100 - 1,
                            99.0);
//                    env->CallVoidMethod(callback, onProgress, progress);
                } else {
                    LOGI("cutting av_interleaved_write_frame fail flush result %d %s", result,
                         av_err2str(result))
                }
            }
        }
        LOGI("cutting encode flush sendResult:%d %s receiveResult:%d ",
             sendResult, av_err2str(sendResult), receiveResult)
        av_packet_unref(receivePacket);
    } while (sendResult >= 0 || receiveResult >= 0);
    av_write_trailer(outFormatContext);
    // 关闭输出文件
    avio_close(outFormatContext->pb);
    avformat_free_context(outFormatContext);
    outFormatContext = nullptr;

    avcodec_close(encodeContext);
    avcodec_free_context(&encodeContext);
    avfilter_graph_free(&filter_graph);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avformat_close_input(&inFormatContext);
    avformat_free_context(inFormatContext);
    inFormatContext = nullptr;
    videoDecoder->release();
    videoDecoder = nullptr;
    LOGI("cutting done!!!")
    progress = 100;
//    env->CallVoidMethod(callback, onProgress, progress);
//    env->CallVoidMethod(callback, onDone);
}