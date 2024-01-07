#include <jni.h>
#include <string>
#include <loghelper.h>
#include <bits/sysconf.h>
#include "../reader/FFVideoReader.h"
#include "libyuv/scale.h"
#include "libyuv/convert.h"
#include "../globals.h"
#include "libyuv/video_common.h"

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/display.h"
#include "libavutil/opt.h"
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
                                                                jdouble end_time,
                                                                jobject out_config) {
    const char *c_srcPath = env->GetStringUTFChars(src_path, nullptr);
    const char *c_desPath = env->GetStringUTFChars(dest_path, nullptr);

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
    int srcFps = av_q2d(inSteram->avg_frame_rate);
    LOGI("decodecContext %d*%d srcFps:%d", decodecContext->width, decodecContext->height, srcFps)
    if (result != 0) {
        LOGE("cutting avcodec_parameters_to_context fail,%d %s", result, av_err2str(result))
        return -1;
    }
    // 根据设备核心数设置线程数
    long threadCount = sysconf(_SC_NPROCESSORS_ONLN);

    decodecContext->thread_count = threadCount > 0 ? threadCount : 1;
    result = avcodec_open2(decodecContext, decoder, NULL);
    if (result != 0) {
        LOGE("cutting avcodec_open2 fail,%d %s", result, av_err2str(result))
        return -1;
    }

    int outWidth = 0;
    int outHeight = 0;
    int cropWidth = 0;
    int cropHeight = 0;
    int outFps = (int) srcFps;

    if (!out_config) {
        outWidth = decodecContext->width;
        outHeight = decodecContext->height;
    } else {
        jclass outConfigClass = env->GetObjectClass(out_config);
        jfieldID outWidthId = env->GetFieldID(outConfigClass, "width", "I");
        outWidth = env->GetIntField(out_config, outWidthId);

        jfieldID outHeightId = env->GetFieldID(outConfigClass, "height", "I");
        outHeight = env->GetIntField(out_config, outHeightId);

        jfieldID cropWidthId = env->GetFieldID(outConfigClass, "cropWidth", "I");
        cropWidth = env->GetIntField(out_config, cropWidthId);

        jfieldID cropHeightId = env->GetFieldID(outConfigClass, "cropHeight", "I");
        cropHeight = env->GetIntField(out_config, cropHeightId);

        jfieldID outFpsId = env->GetFieldID(outConfigClass, "fps", "D");
        outFps = (int) env->GetDoubleField(out_config, outFpsId);

        int videoWidth = decodecContext->width;
        int videoHeight = decodecContext->height;
        bool isVerticalScreen = false;
        isVerticalScreen = videoHeight > videoWidth;
        if (isVerticalScreen) {
            //竖屏
            int temp = outWidth;
            outWidth = outHeight;
            outHeight = temp;

            temp = cropWidth;
            cropWidth = cropHeight;
            cropHeight = temp;
        }
        if (cropHeight != 0 && cropWidth != 0) {
            if (videoWidth < cropWidth) {
                outWidth = cropWidth;
                outHeight = 0;
            }
            if (videoHeight < cropHeight) {
                outHeight = cropHeight;
                outWidth = 0;
            }
        }

        if (outWidth <= 0 && outHeight <= 0) {
            outWidth = videoWidth;
            outHeight = videoHeight;
        } else if (outWidth > 0) { // scale base width
            outWidth += outWidth % 2;
            outHeight = (jint) (1.0 * outWidth * videoHeight / videoWidth);
            outHeight += outHeight % 2;
        } else { // scale base height
            outHeight += outHeight % 2;
            outWidth = (jint) (1.0 * outHeight * videoWidth / videoHeight);
            outWidth += outWidth % 2;
        }

        if (outFps > srcFps) {
            outFps = srcFps;
        }
        LOGI("set out config,video:%d*%d,out:%d*%d,crop:%d*%d,fps:%d,isVerticalScreen:%d",
             videoWidth, videoHeight,
             outWidth, outHeight, cropWidth, cropHeight, outFps, isVerticalScreen)
    }

    AVRational outTimeBase = {1, outFps * TimeBaseDiff};
    LOGI("cutting config,start_time:%lf end_time:%lf fps:%d timeBase:{%d,%d}", start_time, end_time,
         outFps, outTimeBase.num, outTimeBase.den)

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
    char *fpsFilter = new char[strlen(fpsTag), sizeof(outFps)];
    sprintf(fpsFilter, "%s%d", fpsTag, outFps);
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
    outStream->codecpar->codec_id = AV_CODEC_ID_H264;
    outStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    outStream->codecpar->codec_tag = 0;
    outStream->avg_frame_rate = {outFps, 1};
    outStream->time_base = outTimeBase;
    outStream->start_time = 0;
    outStream->codecpar->width = cropWidth != 0 ? cropWidth : outWidth;
    outStream->codecpar->height = cropHeight != 0 ? cropHeight : outHeight;
    av_dict_copy(&outStream->metadata, inSteram->metadata, 0);
    outStream->side_data = inSteram->side_data;
    outStream->nb_side_data = inSteram->nb_side_data;
    //查找解码器
    const AVCodec *encoder = avcodec_find_encoder(outStream->codecpar->codec_id);
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
//    encodeContext->sample_aspect_ratio = decodecContext->sample_aspect_ratio;
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
    LOGI("cutting name:%s", avcodec_get_name(outFormatContext->video_codec_id))
    // 打开输出文件
    if (avio_open(&outFormatContext->pb, c_desPath, AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(outFormatContext, NULL) < 0) {
        LOGE("cutting not open des");
        return -1;
    }

    LOGI("cutting seek %f", start_time / av_q2d(inTimeBase))
    if (start_time > 0) {
        avformat_seek_file(inFormatContext, video_stream_index, INT64_MIN,
                           start_time / av_q2d(inTimeBase), INT64_MAX, 0);
    }

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
            if (frame->pts < start_time / av_q2d(inTimeBase)) {
                LOGI("cutting pts < start_time %ld %f %f", frame->pts,
                     start_time / av_q2d(inTimeBase),
                     frame->pts * av_q2d(inTimeBase))
                av_frame_free(&frame);
                av_packet_free(&packet);
                continue;
            }
            AVFrame *filtered_frame = av_frame_alloc();
            // 将帧发送到filter图中
            int frameFlags = av_buffersrc_add_frame_flags(buffersrcContext, frame,
                                                          AV_BUFFERSRC_FLAG_KEEP_REF);
            int buffersinkGetFrame = av_buffersink_get_frame(buffersinkContext, filtered_frame);
            if (frameFlags <
                0 ||
                buffersinkGetFrame < 0) {
                LOGE("cutting filter frame %d %d", frameFlags, buffersinkGetFrame);
                if (buffersinkGetFrame == AVERROR(EAGAIN)) {
                    av_frame_free(&frame);
                    av_frame_free(&filtered_frame);
                    av_packet_free(&packet);
                    continue;
                }
                return -1;
            }
            LOGI("cutting filterframe %ld(%f)  format:%s",
                 filtered_frame->pts * TimeBaseDiff,
                 filtered_frame->pts * TimeBaseDiff * av_q2d(outTimeBase),
                 av_get_pix_fmt_name((AVPixelFormat) filtered_frame->format))

            if (filtered_frame->pts * TimeBaseDiff * av_q2d(outTimeBase) > end_time) {
                LOGI("cutting pts > endpts")
                av_frame_free(&frame);
                av_frame_free(&filtered_frame);
                av_packet_free(&packet);
                break;
            }
            AVFrame *srcFrame = filtered_frame;

            if (srcFrame->format == AV_PIX_FMT_BGRA) {
                AVFrame *rgbFrame = av_frame_alloc();
                rgbFrame->width = srcFrame->width;
                rgbFrame->height = srcFrame->height;
                rgbFrame->format = AV_PIX_FMT_YUV420P;
                rgbFrame->pts = srcFrame->pts;
                rgbFrame->pkt_dts = srcFrame->pkt_dts;
                rgbFrame->duration = srcFrame->duration;
                rgbFrame->pkt_size = srcFrame->pkt_size;
                int ret = av_frame_get_buffer(rgbFrame, 0);
                LOGI("cutting convert av_frame_get_buffer %d", ret)
                //https://www.v2ex.com/t/756129
                //bgra顺序,需要反过来调用argb
                ret = libyuv::ARGBToI420(srcFrame->data[0], srcFrame->linesize[0],
                                         rgbFrame->data[0], rgbFrame->linesize[0],
                                         rgbFrame->data[1], rgbFrame->linesize[1],
                                         rgbFrame->data[2], rgbFrame->linesize[2],
                                         rgbFrame->width, rgbFrame->height);
                av_frame_free(&srcFrame);
                srcFrame = nullptr;
                srcFrame = rgbFrame;
            }

            if (outWidth != srcFrame->width || outHeight != srcFrame->height) {
                AVFrame *scaleFrame = av_frame_alloc();
                scaleFrame->width = outWidth;
                scaleFrame->height = outHeight;
                scaleFrame->format = srcFrame->format;

                scaleFrame->pts = srcFrame->pts;
                scaleFrame->pkt_dts = srcFrame->pkt_dts;
                scaleFrame->duration = srcFrame->duration;
                scaleFrame->pkt_size = srcFrame->pkt_size;
                av_frame_get_buffer(scaleFrame, 0);

                libyuv::I420Scale(srcFrame->data[0], srcFrame->linesize[0],
                                  srcFrame->data[1], srcFrame->linesize[1],
                                  srcFrame->data[2], srcFrame->linesize[2],
                                  srcFrame->width, srcFrame->height,
                                  scaleFrame->data[0], scaleFrame->linesize[0],
                                  scaleFrame->data[1], scaleFrame->linesize[1],
                                  scaleFrame->data[2], scaleFrame->linesize[2],
                                  outWidth, outHeight, libyuv::kFilterNone);
                av_frame_free(&srcFrame);
                srcFrame = nullptr;
                srcFrame = scaleFrame;
            }

            bool needCrop = false;
            if (cropWidth != 0 && cropHeight != 0 &&
                (cropWidth < srcFrame->width || cropHeight < srcFrame->height)) {
                needCrop = true;
            }
            if (needCrop) {
                int srcWidth = srcFrame->width;
                int srcHeight = srcFrame->height;
                AVFrame *cropFrame = av_frame_alloc();
                cropFrame->width = cropWidth;
                cropFrame->height = cropHeight;
                cropFrame->format = srcFrame->format;

                cropFrame->pts = srcFrame->pts;
                cropFrame->pkt_dts = srcFrame->pkt_dts;
                cropFrame->duration = srcFrame->duration;
                int ret = av_frame_get_buffer(cropFrame, 0);

                int diffWidth = srcWidth - cropWidth;
                int dx = diffWidth / 2 + diffWidth % 2;

                int diffHeight = srcHeight - cropHeight;
                int dy = diffHeight / 2 + diffHeight % 2;
                LOGI("[video] decode av_frame_get_buffer:%d src:%d*%d d:%d-%d crop:%d*%d", ret,
                     srcWidth,
                     srcHeight, dx, dy,
                     cropWidth, cropHeight)

                int size = av_image_get_buffer_size((AVPixelFormat) srcFrame->format,
                                                    srcFrame->width, srcFrame->height, 1);
                auto *buffer = static_cast<uint8_t *>(av_malloc(size * sizeof(uint8_t)));

                ret = av_image_copy_to_buffer(buffer, size, srcFrame->data, srcFrame->linesize,
                                              (AVPixelFormat) srcFrame->format,
                                              srcFrame->width, srcFrame->height, 1);

                LOGI("[video] decode av_image_copy_to_buffer %d %d", ret, size)

                ret = libyuv::ConvertToI420(buffer, size,
                                            cropFrame->data[0], cropFrame->linesize[0],
                                            cropFrame->data[1], cropFrame->linesize[1],
                                            cropFrame->data[2], cropFrame->linesize[2],
                                            dx, dy,
                                            srcWidth, srcHeight,
                                            cropWidth, cropHeight,
                                            libyuv::kRotate0, libyuv::FOURCC_I420
                );
                LOGI("[video] decode ConvertToI420:%d", ret)
                av_free(buffer);
                av_frame_free(&srcFrame);
                srcFrame = nullptr;
                srcFrame = cropFrame;
            }

            srcFrame->pict_type = AV_PICTURE_TYPE_NONE;
            do {
                sendResult = -1;
                receiveResult = -1;
                sendResult = avcodec_send_frame(encodeContext, srcFrame);
                if (sendResult == 0) {
                    AVPacket *receivePacket = av_packet_alloc();
                    receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
                    if (receiveResult == 0) {
                        receivePacket->stream_index = video_stream_index;
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
            av_frame_free(&frame);
            av_frame_free(&srcFrame);
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
        if (sendResult == 0 || sendResult == AVERROR_EOF) {
            receiveResult = avcodec_receive_packet(encodeContext, receivePacket);
            if (receiveResult == 0) {
                receivePacket->stream_index = video_stream_index;
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

                int result = av_write_frame(outFormatContext,
                                            receivePacket);
                if (result == 0) {
                    writeFramsCount += TimeBaseDiff;
                } else {
                    LOGI("cutting av_interleaved_write_frame fail flush result %d %s", result,
                         av_err2str(result))
                }
            }
        }
        LOGI("cutting encode flush sendResult:%d %s receiveResult:%d ",
             sendResult, av_err2str(sendResult), receiveResult)
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