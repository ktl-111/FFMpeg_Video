#include <jni.h>
#include <string>
#include <loghelper.h>
#include "../reader/FFVideoReader.h"

extern "C" {
#include "libavutil/time.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

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

    // 打开输入文件
    AVFormatContext *format_context = avformat_alloc_context();
    if (avformat_open_input(&format_context, c_srcPath, NULL, NULL) != 0) {
        LOGE("无法打开输入文件\n");
        return -1;
    }

    // 获取流信息
    if (avformat_find_stream_info(format_context, NULL) < 0) {
        LOGE("无法获取流信息\n");
        return -1;
    }

    // 找到视频流的索引
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOGE("找不到视频流\n");
        return -1;
    }

    // 获取视频流的解码器参数
    AVCodecParameters *codec_params = format_context->streams[video_stream_index]->codecpar;

    // 找到解码器
    const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        LOGE("找不到解码器\n");
        return -1;
    }

    // 打开解码器
    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_context, codec_params) < 0 ||
        avcodec_open2(codec_context, codec, NULL) < 0) {
        LOGE("无法打开解码器\n");
        return -1;
    }
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (!buffersrc || !buffersink) {
        LOGE("buffersrc  buffersink fail")
        return -1;
    }
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        LOGE("avfilter_graph_alloc fail")
        return -1;
    }

    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    char args[512];
    int ret = 0;
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    AVRational &timeBase = format_context->streams[video_stream_index]->time_base;
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             codec_context->width, codec_context->height, codec_context->pix_fmt,
             timeBase.num,
             timeBase.den,
             codec_context->sample_aspect_ratio.num, codec_context->sample_aspect_ratio.den);
    LOGI("args %s", args)
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        LOGE("buffersrc fail %d %s", ret, av_err2str(ret))
        return -1;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        LOGE("buffersink fail")
        return -1;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
// 设置filter图的输入输出
    if (avfilter_graph_parse_ptr(filter_graph, "fps=30", &inputs, &outputs, NULL) < 0 ||
        avfilter_graph_config(filter_graph, NULL) < 0) {
        LOGE("无法配置filter图\n");
        return -1;
    }

    // 创建输出文件
    AVFormatContext *output_format_context = avformat_alloc_context();
    if (avformat_alloc_output_context2(&output_format_context, NULL, NULL, c_desPath) < 0) {
        LOGE("无法创建输出文件\n");
        return -1;
    }

    // 添加视频流
    AVStream *output_video_stream = avformat_new_stream(output_format_context, NULL);
    if (!output_video_stream) {
        LOGE("无法创建输出视频流\n");
        return -1;
    }

    // 复制解码器参数到输出流
    if (avcodec_parameters_copy(output_video_stream->codecpar, codec_params) < 0) {
        LOGE("无法复制解码器参数\n");
        return -1;
    }
    output_video_stream->codecpar->codec_tag = 0;
    output_video_stream->avg_frame_rate = {30, 1};
    output_video_stream->time_base = {1, 30};
    // 打开输出文件
    if (avio_open(&output_format_context->pb, c_desPath, AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(output_format_context, NULL) < 0) {
        LOGE("无法打开输出文件\n");
        return -1;
    }

    // 初始化解码过程
    while (1) {
        AVPacket *packet = av_packet_alloc();
        int result = av_read_frame(format_context, packet);
        if (result != 0) {
            LOGE("cutting av_read_frame fail %d", result)
            continue;
        }
        if (packet->stream_index == video_stream_index) {

            // 初始化filter图的帧
            AVFrame *frame = av_frame_alloc();
            AVFrame *filtered_frame = av_frame_alloc();
            if (avcodec_send_packet(codec_context, packet) == 0 &&
                avcodec_receive_frame(codec_context, frame) == 0) {
                LOGI("pts:%ld endPts:%f", frame->pts, 5 / av_q2d(timeBase))
                if (frame->pts > 5 / av_q2d(timeBase)) {
                    break;
                }
                frame->pts = frame->best_effort_timestamp;
                // 将帧发送到filter图中

                int frameFlags = av_buffersrc_add_frame_flags(buffersrc_ctx, frame,
                                                              AV_BUFFERSRC_FLAG_KEEP_REF);
                int buffersinkGetFrame = av_buffersink_get_frame(buffersink_ctx, filtered_frame);
                if (frameFlags <
                    0 ||
                    buffersinkGetFrame < 0) {
                    LOGE("无法通过filter图处理帧 %d %d", frameFlags, buffersinkGetFrame);
                    if (buffersinkGetFrame == AVERROR(EAGAIN)) {
                        continue;
                    }
                    return -1;
                }
                result = av_interleaved_write_uncoded_frame(output_format_context,
                                                            video_stream_index, filtered_frame);
                if (result < 0) {
                    LOGE("cutting av_interleaved_write_uncoded_frame %d %s", result,
                         av_err2str(result))
                    return -1;
                }
                // 清理资源
                av_frame_free(&frame);
                av_frame_free(&filtered_frame);
                // 将filter后的帧写入输出文件
//                if (av_write_frame(output_format_context, packet) < 0) {
//                    LOGE("无法写入帧到输出文件\n");
//                    return -1;
//                }
                LOGI("av_write_frame")
            }
        }

        av_packet_unref(packet);
    }

    avcodec_free_context(&codec_context);
    avfilter_graph_free(&filter_graph);
    avformat_close_input(&format_context);
// 释放资源
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    // 写入输出文件的尾部
    av_write_trailer(output_format_context);

    // 关闭输出文件
    avio_close(output_format_context->pb);

    LOGI("cutting done!!!");
    return 0;
}