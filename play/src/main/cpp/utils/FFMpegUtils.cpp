#include <jni.h>
#include <string>
#include <loghelper.h>
#include "../reader/FFVideoReader.h"
#include "libyuv/scale.h"

extern "C" {
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
Java_com_example_play_utils_FFMpegUtils_nativeCutting(JNIEnv *env, jobject thiz,
                                                      jstring src_path, jstring dest_path,
                                                      jdouble start_time,
                                                      jdouble end_time, jint fps) {
    const char *c_srcPath = env->GetStringUTFChars(src_path, nullptr);
    const char *c_desPath = env->GetStringUTFChars(dest_path, nullptr);
    int timeBaseDiff = 1500;
    int scale = 2;
    AVRational outTimeBase = {1, fps * timeBaseDiff};
    LOGI("cutting config,start_time:%lf end_time:%lf fps:%d timeBase:{%d,%d}", start_time, end_time,
         fps, outTimeBase.num, outTimeBase.den)

    AVFormatContext *inFormatContext = avformat_alloc_context();
    int result = 0;
    result = avformat_open_input(&inFormatContext, c_srcPath, NULL, NULL);
    if (result != 0) {
        LOGE("cutting not open input file,%d %s", result, av_err2str(result));
        return -1;
    }

    if (avformat_find_stream_info(inFormatContext, NULL) < 0) {
        LOGE("cutting avformat_find_stream_info fail");
        return -1;
    }

    int video_stream_index = av_find_best_stream(inFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                 nullptr, 0);
    if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
        LOGI("cutting not find video index,%d %s", result, av_err2str(result))
        return -1;
    }

    AVStream *inSteram = inFormatContext->streams[video_stream_index];
    AVRational &inTimeBase = inSteram->time_base;
    AVCodecParameters *codec_params = inSteram->codecpar;
    const AVCodec *decoder = avcodec_find_decoder(codec_params->codec_id);
    if (!decoder) {
        LOGE("cutting not find decoder,%d %s", result, av_err2str(result))
        return -1;
    }

    AVCodecContext *decodecContext = avcodec_alloc_context3(decoder);
    result = avcodec_parameters_to_context(decodecContext, codec_params);
    int dstWidth = decodecContext->width / scale;
    int dstHeight = decodecContext->height / scale;
    LOGI("decodecContext %d*%d ", decodecContext->width, decodecContext->height)
    if (result != 0) {
        LOGE("cutting avcodec_parameters_to_context fail,%d %s", result, av_err2str(result))
        return -1;
    }

    result = avcodec_open2(decodecContext, decoder, NULL);
    if (result != 0) {
        LOGE("cutting avcodec_open2 fail,%d %s", result, av_err2str(result))
        return -1;
    }


    //设置filter
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!buffersrc || !buffersink || !filter_graph) {
        LOGE("cutting buffersrc  buffersink avfilter_graph_alloc fail")
        return -1;
    }
    AVFilterContext *buffersinkContext;
    AVFilterContext *buffersrcContext;
    char args[512];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             decodecContext->width, decodecContext->height, decodecContext->pix_fmt,
             inTimeBase.num,
             inTimeBase.den,
             decodecContext->sample_aspect_ratio.num, decodecContext->sample_aspect_ratio.den);
    LOGI("cutting args %s", args)
    result = avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in",
                                          args, NULL, filter_graph);

    if (result != 0) {
        LOGE("cutting buffersrc fail,%d %s", result, av_err2str(result))
        return -1;
    }

    result = avfilter_graph_create_filter(&buffersinkContext, buffersink, "out",
                                          NULL, NULL, filter_graph);
    if (result != 0) {
        LOGE("cutting buffersink fail,%d %s", result, av_err2str(result))
        return -1;
    }
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
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
    char *fpsFilter = new char[strlen(fpsTag), sizeof(fps)];
    sprintf(fpsFilter, "%s%d", fpsTag, fps);
    LOGI("cutting fpsfilter:%s", fpsFilter)
    //buffer->输出(outputs)->filter in(av_strdup("in")->fps filter->filter out(av_strdup("out"))->输入(inputs)->buffersink
    result = avfilter_graph_parse_ptr(filter_graph, fpsFilter, &inputs, &outputs, NULL);
    if (result != 0) {
        LOGE("cutting avfilter_graph_parse_ptr fail,%d %s", result, av_err2str(result))
        return -1;
    }
    result = avfilter_graph_config(filter_graph, NULL);
    if (result != 0) {
        LOGE("cutting avfilter_graph_config fail,%d %s", result, av_err2str(result))
        return -1;
    }


    AVFormatContext *outFormatContext = avformat_alloc_context();
    result = avformat_alloc_output_context2(&outFormatContext, NULL, NULL, c_desPath);
    if (result != 0) {
        LOGE("cutting avformat_alloc_output_context2 fail,%d %s", result, av_err2str(result))
        return -1;
    }

    AVStream *outStream = avformat_new_stream(outFormatContext, NULL);
    if (!outStream) {
        LOGE("cutting avformat_new_stream fail,%d %s", result, av_err2str(result))
        return -1;
    }
    if (avcodec_parameters_copy(outStream->codecpar, codec_params) < 0) {
        LOGE("cutting avcodec_parameters_copy fail,%d %s", result, av_err2str(result))
        return -1;
    }

    //设置对应参数
    outStream->codecpar->codec_tag = 0;
    outStream->avg_frame_rate = {fps, 1};
    outStream->time_base = outTimeBase;
    outStream->start_time = 0;
    outStream->codecpar->width = dstWidth;
    outStream->codecpar->height = dstHeight;
    av_dict_copy(&outStream->metadata, inSteram->metadata, 0);
    outStream->side_data = inSteram->side_data;
    outStream->nb_side_data = inSteram->nb_side_data;
    //查找解码器
    const AVCodec *encoder = avcodec_find_encoder(codec_params->codec_id);
    if (!encoder) {
        LOGE("not find encoder")
        return -1;
    }
    LOGI("find encoder %s", encoder->name)
    AVCodecContext *encodeContext = avcodec_alloc_context3(encoder);
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
    encodeContext->sample_aspect_ratio = decodecContext->sample_aspect_ratio;
    encodeContext->max_b_frames = 0;//不需要B帧
    encodeContext->gop_size = outStream->avg_frame_rate.num;//多少帧一个I帧

    LOGI("cutting config sample_aspect_ratio:{%d,%d}", encodeContext->sample_aspect_ratio.num,
         encodeContext->sample_aspect_ratio.den)
    result = avcodec_open2(encodeContext, encoder, NULL);
    if (result != 0) {
        LOGI("cutting avcodec_open2 fail,%d %s", result, av_err2str(result))
        return -1;
    }

    result = avcodec_parameters_from_context(outStream->codecpar, encodeContext);
    if (result < 0) {
        LOGE("cutting avcodec_parameters_from_context fail,%d %s", result, av_err2str(result))
        return -1;
    }

    // 打开输出文件
    if (avio_open(&outFormatContext->pb, c_desPath, AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(outFormatContext, NULL) < 0) {
        LOGE("cutting not open des");
        return -1;
    }

    LOGI("cutting seek")
    av_seek_frame(inFormatContext, video_stream_index, start_time / av_q2d(inTimeBase),
                  AVSEEK_FLAG_ANY);

    int writeFramsCount = 0;
    while (1) {
        AVPacket *packet = av_packet_alloc();
        int result = av_read_frame(inFormatContext, packet);
        if (result != 0) {
            LOGE("cutting av_read_frame fail %d %s", result, av_err2str(result))
            break;
        }
        if (packet->stream_index == video_stream_index) {
            // 初始化filter图的帧
            AVFrame *frame = av_frame_alloc();
            int sendResult = -1;
            int receiveResult = -1;
            LOGI("cutting send pts:%ld %f dts:%ld %f duration:%ld %f flags:%d ", packet->pts,
                 packet->pts * av_q2d(inTimeBase),
                 packet->dts, packet->dts * av_q2d(inTimeBase),
                 packet->duration, packet->duration * av_q2d(inTimeBase),
                 packet->flags)
            do {
                sendResult = avcodec_send_packet(decodecContext, packet);
                if (sendResult == 0 || sendResult == AVERROR(EAGAIN)) {
                    receiveResult = avcodec_receive_frame(decodecContext, frame);
                    if (receiveResult == 0) {
                        LOGI("cutting receiveResult frame pts:%ld %lf,dts:%ld %d*%d", frame->pts,
                             frame->pts * av_q2d(inTimeBase),
                             frame->pkt_dts,
                             frame->width, frame->height)
                    }
                }
                LOGI("cutting decode %d %d", sendResult, receiveResult)
            } while (sendResult == AVERROR(EAGAIN) && receiveResult == AVERROR(EAGAIN));
            if (receiveResult == AVERROR(EAGAIN)) {
                av_frame_unref(frame);
                av_packet_unref(packet);
                continue;
            }

            if (frame->pts > end_time / av_q2d(inTimeBase)) {
                LOGI("cutting pts > endpts %ld %f %f", frame->pts, 5 / av_q2d(inTimeBase),
                     frame->pts * av_q2d(inTimeBase))
                av_frame_unref(frame);
                av_packet_unref(packet);
                break;
            }

            AVFrame *filtered_frame = av_frame_alloc();
            // 将帧发送到filter图中
            int frameFlags = av_buffersrc_add_frame_flags(buffersrcContext, frame,
                                                          AV_BUFFERSRC_FLAG_KEEP_REF);
            int buffersinkGetFrame = av_buffersink_get_frame(buffersinkContext, filtered_frame);
            if (frameFlags <
                0 ||
                buffersinkGetFrame < 0) {
                LOGE("无法通过filter图处理帧 %d %d", frameFlags, buffersinkGetFrame);
                if (buffersinkGetFrame == AVERROR(EAGAIN)) {
                    av_frame_unref(frame);
                    av_frame_unref(filtered_frame);
                    av_packet_unref(packet);
                    continue;
                }
                return -1;
            }
            do {
                sendResult = -1;
                receiveResult = -1;
                AVPacket *receivePacket = av_packet_alloc();
                filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
                AVFrame *dstFrame = av_frame_alloc();
                av_frame_ref(dstFrame, filtered_frame);
                int64_t scaleBufferSize = dstWidth * dstHeight * 3 / 2;
                uint8_t *scaleBuffer = (uint8_t *) malloc(scaleBufferSize);

                int ret = libyuv::I420Scale(
                        filtered_frame->data[0], filtered_frame->linesize[0],
                        filtered_frame->data[1], filtered_frame->linesize[1],
                        filtered_frame->data[2], filtered_frame->linesize[2],
                        filtered_frame->width, filtered_frame->height,
                        scaleBuffer, dstWidth,
                        scaleBuffer + dstWidth * dstHeight, dstWidth / 2,
                        scaleBuffer + dstWidth * dstHeight * 5 / 4, dstWidth / 2,
                        dstWidth, dstHeight, libyuv::kFilterNone);
                LOGI("cutting I420Scale %d", ret)
                dstFrame->data[0] = scaleBuffer;
                dstFrame->data[1] = scaleBuffer + dstWidth * dstHeight;
                dstFrame->data[2] = scaleBuffer + dstWidth * dstHeight * 5 / 4;
                dstFrame->linesize[0] = dstWidth;
                dstFrame->linesize[1] = dstWidth / 2;
                dstFrame->linesize[2] = dstWidth / 2;
                dstFrame->width = dstWidth;
                dstFrame->height = dstHeight;
                LOGI("cutting I420Scale %d*%d->%d*%d %f", filtered_frame->width,
                     filtered_frame->height, dstFrame->width, dstFrame->height, dstFrame->pts *
                                                                                av_q2d(outTimeBase))
                sendResult = avcodec_send_frame(encodeContext, dstFrame);
                if (sendResult == 0) {
                    receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
                    if (receiveResult == 0) {
                        receivePacket->stream_index = video_stream_index;
                        receivePacket->pts = writeFramsCount;
                        receivePacket->dts = receivePacket->pts - timeBaseDiff;
                        receivePacket->duration = timeBaseDiff;

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
                            writeFramsCount += timeBaseDiff;
                        } else {
                            LOGI("cutting av_interleaved_write_frame fail result %d %s", result,
                                 av_err2str(result))
                        }
                    }
                }
                LOGI("cutting encode sendResult:%d receiveResult:%d", sendResult, receiveResult)
                free(scaleBuffer);
                av_frame_free(&dstFrame);
                av_packet_free(&receivePacket);
            } while (sendResult == AVERROR(EAGAIN) && receiveResult == AVERROR(EAGAIN));
            // 清理资源
            av_frame_free(&frame);
            av_frame_free(&filtered_frame);
            av_packet_free(&packet);
            if (receiveResult == AVERROR(EAGAIN)) {
                continue;
            }
            LOGI("cutting av_write_frame")
        } else {
            av_packet_unref(packet);
        }

    }

    int sendResult = -1;
    int receiveResult = -1;
    do {
        sendResult = -1;
        receiveResult = -1;
        AVPacket *receivePacket = av_packet_alloc();
        sendResult = avcodec_send_frame(encodeContext, nullptr);
        if (sendResult == 0 || sendResult == AVERROR_EOF || sendResult == AVERROR_EOF) {
            receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
            if (receiveResult == 0) {
                receivePacket->stream_index = video_stream_index;
                receivePacket->pts = writeFramsCount;
                receivePacket->dts = receivePacket->pts;
                receivePacket->duration = timeBaseDiff;

                LOGI("cutting av_interleaved_write_frame result pts:%ld %f dts:%ld %f duration:%ld %f",
                     receivePacket->pts,
                     receivePacket->pts * av_q2d(encodeContext->time_base),
                     receivePacket->dts,
                     receivePacket->dts * av_q2d(encodeContext->time_base),
                     receivePacket->duration,
                     receivePacket->duration * av_q2d(encodeContext->time_base))

                int result = av_write_frame(outFormatContext,
                                            receivePacket);
                if (result == 0) {
                    writeFramsCount += timeBaseDiff;
                } else {
                    LOGI("cutting av_interleaved_write_frame fail result %d %s", result,
                         av_err2str(result))
                }
            }
        }
        LOGI("cutting encode flush sendResult:%d %s receiveResult:%d AVERROR_EOF:%d AVERROR(EAGAIN):%d",
             sendResult, av_err2str(sendResult), receiveResult, AVERROR_EOF, AVERROR(EAGAIN))
        av_packet_unref(receivePacket);
    } while (receiveResult >= 0);
    av_write_trailer(outFormatContext);
    // 关闭输出文件
    avio_close(outFormatContext->pb);

    avcodec_free_context(&decodecContext);
    avformat_close_input(&inFormatContext);
    avfilter_graph_free(&filter_graph);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    env->ReleaseStringUTFChars(src_path, c_srcPath);
    env->ReleaseStringUTFChars(dest_path, c_desPath);

    LOGI("cutting done!!!");
    return true;
}