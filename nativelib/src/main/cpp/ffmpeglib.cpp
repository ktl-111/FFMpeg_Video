#include <jni.h>
#include "loghelper.h"
#include "Queue.h"
#include <pthread.h>
#include <unistd.h>

#include <android/native_window_jni.h>

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
}

_JavaVM *javaVM = nullptr;
jobject instance = nullptr;
jmethodID onCallData = nullptr;
extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jint result = -1;
    javaVM = vm;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return result;
    }
    return JNI_VERSION_1_4;
}

AVFormatContext *avFormatContext;
AVCodecContext *videoContext;
AVCodecContext *audioContext;
//等同于rgbframe.data,两个指针指的是同一个位置,由于后面copy时需要移动指针,但是rgbFrame.data需要用来存数据,不允许移动指针
//所以定义个影子buffer
uint8_t *shadowedOutbuffer;
AVFrame *rgbFrame;
SwsContext *videoSwsContext;
Queue *videoQueue;
Queue *audioQueue;
int videoIndex;
int audioIndex;
int videoSleep;

SLAndroidSimpleBufferQueueItf pcmBufferQueue;
uint8_t *audioOutBuffer;
enum AVPixelFormat format = AV_PIX_FMT_RGBA;

ANativeWindow *nativeWindow;
ANativeWindow_Buffer windowBuffer;

AVFormatContext *outFormatContext;

void initVideo();

void *decodePacket(void *params);

void *decodeVideo(void *params);

void freePacket(AVPacket **pPacket);

void freeFrame(AVFrame **pAvFrame);

void initAudio();

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context);

void startLooprtDecode();

int sampleConvertOpensles(int sample_rate);

int initMuxerParams(char *destPath);

bool startDecode = false;
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_nativelib_FFMpegPlay_play(JNIEnv *env, jobject thiz, jstring url_,
                                           jobject surface) {
    instance = env->NewGlobalRef(thiz);
    jclass jclazz = env->GetObjectClass(instance);
    onCallData = env->GetMethodID(jclazz, "onCallData", "([B)V");

    char *url = const_cast<char *>(env->GetStringUTFChars(url_, 0));

    LOGI("play url:%s", url);
    //注册所有组件
    av_register_all();
    if (url[0] == 'h') {
        LOGI("play init network");
        //初始化网络模块
        avformat_network_init();
    }
    //分配 avFormatContext
    avFormatContext = avformat_alloc_context();
    //打开文件输入流
    avformat_open_input(&avFormatContext, url, nullptr, nullptr);
    //提取输入文件中的数据流信息
    int code = avformat_find_stream_info(avFormatContext, nullptr);

    if (code < 0) {
        LOGE("avformat_find_stream_info code:%d", code);
        env->ReleaseStringUTFChars(url_, url);
        return false;
    }

    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        AVStream *pStream = avFormatContext->streams[i];
        AVCodecParameters *codecpar = pStream->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //视频流
            videoIndex = i;
            //查找对应的编解码器,传入获取到的id
            AVCodec *avCodec = avcodec_find_decoder(codecpar->codec_id);
            //分配解码器分配上下文
            videoContext = avcodec_alloc_context3(avCodec);
            //将数据流相关的编解码参数来填充编解码器上下文
            avcodec_parameters_to_context(videoContext, codecpar);
            //打开解码器
            avcodec_open2(videoContext, avCodec, 0);

            int fps = av_q2d(pStream->avg_frame_rate);
            videoSleep = 1000 / fps;
            LOGI("视频流index:%d,width:%d,height:%d,fps:%d,duration:%lld,codecName:%s", i,
                 codecpar->width,
                 codecpar->height,
                 fps, avFormatContext->duration / AV_TIME_BASE, avCodec->name);


        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频流
            audioIndex = i;
            //查找对应的编解码器,传入获取到的id
            AVCodec *avCodec = avcodec_find_decoder(codecpar->codec_id);
            //分配解码器分配上下文
            audioContext = avcodec_alloc_context3(avCodec);
            //将数据流相关的编解码参数来填充编解码器上下文
            avcodec_parameters_to_context(audioContext, codecpar);
            //打开解码器
            avcodec_open2(audioContext, avCodec, 0);

            LOGI("音频流index:%d,sample_rate:%d,channels:%d,frame_size:%d", i, codecpar->sample_rate,
                 codecpar->channels, codecpar->frame_size);
        } else {
            LOGI("其他流index:%d,codec_type:%d", i, codecpar->codec_type);
        }
    }

    //回调给应用层
    jclass instance = env->GetObjectClass(thiz);
    jmethodID jmethodId = env->GetMethodID(instance, "onConfig", "(II)V");
    env->CallVoidMethod(thiz, jmethodId, videoContext->width, videoContext->height);

    //获取缓冲window
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    //修改缓冲区格式和大小,对应视频格式和大小
    ANativeWindow_setBuffersGeometry(nativeWindow, videoContext->width, videoContext->height,
                                     WINDOW_FORMAT_RGBA_8888);

    startDecode = true;

    initVideo();
    initAudio();
    startLooprtDecode();
    env->ReleaseStringUTFChars(url_, url);
    return false;
}

void initAudio() {
    /**
     * opensl es 使用三步骤
     * 1.创建 create
     * 2.实例化 realize
     * 3.使用对应接口 GetInterface(xxx)
     * */
    SLObjectItf engine;
    //获取引擎
    auto result = slCreateEngine(&engine, 0, 0, 0, 0, 0);

    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio init fail %d", result);
        return;
    }
    SLEngineItf engineObject;
    //实例化引擎,false:同步,true:异步
    result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio engine Realize fail %d", result);
        return;
    }
    //执行获取引擎对象接口
    result = (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineObject);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_ENGINE fail %d", result);
        return;
    }

    //创建多路混音器
    SLObjectItf mix;
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineObject)->CreateOutputMix(engineObject, &mix, 1, mids, mreq);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio CreateOutputMix fail %d", result);
        return;
    }
    //实例化混音器
    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio mix Realize failed %d", result);
        return;
    }

    //配置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSnk = {&outputMix, nullptr};

    //创建pcm 配置
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,//pcm格式
            static_cast<SLuint32>(audioContext->channels),//声道
            static_cast<SLuint32>(sampleConvertOpensles(audioContext->sample_rate)),//采样率
            SL_PCMSAMPLEFORMAT_FIXED_16,//bits
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    //定位要播放声音数据的存放位置
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataSource audioSrc = {&que, &pcm};

    //创建播放器
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE};
    SLObjectItf audioPlayObject;
    result = (*engineObject)->CreateAudioPlayer(engineObject, &audioPlayObject, &audioSrc,
                                                &audioSnk, 1,
                                                ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio CreateAudioPlayer failed %d", result);
        return;
    }

    //实例化
    result = (*audioPlayObject)->Realize(audioPlayObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio audioPlayObject Realize failed %d", result);
        return;
    }

    //获取操作对象
    SLPlayItf operateObject;
    result = (*audioPlayObject)->GetInterface(audioPlayObject, SL_IID_PLAY, &operateObject);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_PLAY failed %d", result);
        return;
    }

    //使用接口,获取注册缓冲区回调对象
    result = (*audioPlayObject)->GetInterface(audioPlayObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("initAudio SL_IID_BUFFERQUEUE fail %d", result);
        return;
    }
    //注册回调
    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, pcmBufferCallBack, 0);

    //设置播放
    (*operateObject)->SetPlayState(operateObject, SL_PLAYSTATE_PLAYING);

    //采样率*通道数*16bit(2字节)
    audioOutBuffer = (uint8_t *) av_malloc(audioContext->sample_rate * audioContext->channels * 2);
    //启动回调
    (*pcmBufferQueue)->Enqueue(pcmBufferQueue, "", 1);
    LOGI("initAudio done");
}

void initVideo() {
    //初始存放目标数据的化容器
    rgbFrame = av_frame_alloc();
    //获取容器大小 格式,宽高
    int buffSize = av_image_get_buffer_size(format, videoContext->width, videoContext->height, 1);
    //分配内存空间给容器
    shadowedOutbuffer = static_cast<uint8_t *>(av_malloc(buffSize * sizeof(uint8_t)));
    //将rgbFrame->data指针指向outbuffer,等同于rgbFrame->data=shadowedOutbuffer
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, shadowedOutbuffer, format,
                         videoContext->width, videoContext->height, 1);

    //初始化转换上下文,原视频宽高+format,目标视频宽高+format
    videoSwsContext = sws_getContext(videoContext->width, videoContext->height,
                                     videoContext->pix_fmt,
                                     videoContext->width, videoContext->height, format,
                                     SWS_BICUBIC, nullptr, nullptr, nullptr);
}

void startLooprtDecode() {
    videoQueue = new Queue();
    audioQueue = new Queue();
    //执行解码线程
    pthread_t thread_decode;
    pthread_create(&thread_decode, nullptr, decodePacket, nullptr);

    //创建生产消费者,loop线程
    pthread_t thread_vidio;
    pthread_create(&thread_vidio, nullptr, decodeVideo, nullptr);
}

void *decodePacket(void *params) {
    LOGI("读取数据包");
    while (startDecode) {
        LOGI("start read");
        //创建packet接收
        AVPacket *avPacket = av_packet_alloc();
        //读取数据
        int result = av_read_frame(avFormatContext, avPacket);
        if (result < 0) {
            LOGI("read done!!!");
            freePacket(&avPacket);
            //结尾
            break;
        }
        if (avPacket->stream_index == videoIndex) {
            //延迟帧率ms
//            usleep(videoSleep * 1000);
            LOGI("视频包大小:%d", avPacket->size);
            videoQueue->push(avPacket);
        } else if (avPacket->stream_index == audioIndex) {
            LOGI("音频包大小:%d", avPacket->size);
            audioQueue->push(avPacket);
        }
    }
    return nullptr;
}

//需要播放时会回调该函数拿数据
void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {
    LOGI("audio pcmBufferCallBack start");
    int dataSize = 0;
    while (startDecode) {
        AVPacket *avPacket = av_packet_alloc();
        LOGI("audio start get data");
        //消费者获取packet
        audioQueue->get(avPacket);
        LOGI("audio 消费者packet size:%d", avPacket->size);

        //通知解码
        int result = avcodec_send_packet(audioContext, avPacket);
        if (result != 0) {
            LOGE("audio avcodec_send_packet error code:%d", result);
            freePacket(&avPacket);
            continue;
        }
        LOGI("audio start receive frame");
        auto startTime = std::chrono::steady_clock::now();
        //创建接收帧容器
        AVFrame *audioFrame = av_frame_alloc();
        //接收解码器输出的数据
        result = avcodec_receive_frame(audioContext, audioFrame);
        if (result != 0) {
            freeFrame(&audioFrame);
            freePacket(&avPacket);
            LOGE("audio avcodec_receive_frame error code:%d", result);
            continue;
        }
        //设置解析option
        SwrContext *swr_ctx = swr_alloc_set_opts(
                NULL,
                AV_CH_LAYOUT_STEREO,
                AV_SAMPLE_FMT_S16,
                audioFrame->sample_rate,
                audioFrame->channel_layout,
                (AVSampleFormat) audioFrame->format,
                audioFrame->sample_rate,
                NULL, NULL
        );

        if (!swr_ctx || swr_init(swr_ctx) < 0) {
            freeFrame(&audioFrame);
            freePacket(&avPacket);
            LOGE("audio swr_ctx init fail %p", &swr_ctx);
            continue;
        }

        //开始转换
        int nb = swr_convert(swr_ctx, &audioOutBuffer, audioFrame->nb_samples,
                             (const uint8_t **) audioFrame->data, audioFrame->nb_samples);
        if (nb < 0) {
            freeFrame(&audioFrame);
            freePacket(&avPacket);
            swr_free(&swr_ctx);
            LOGE("audio swr_convert fail %d", nb);
            continue;
        }

        int outChannel = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        LOGI("audio swr_convert nb:%d outChannel:%d", nb, outChannel);
        //计算转换后的真实大小
        dataSize = nb * outChannel * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        LOGI("audio swr_convert dataSize:%d", dataSize);

        auto endTime = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration<double, std::micro>(endTime - startTime).count();

        LOGI("audio 解码耗时 by micro:%f sample_rate:%d,channels:%d,format:%d,", diff,
             audioFrame->sample_rate, audioFrame->channels, audioFrame->format);

        freeFrame(&audioFrame);
        freePacket(&avPacket);
        swr_free(&swr_ctx);
        break;
    }
    if (dataSize > 0) {
        LOGI("audio pcmBufferQueue Enqueue");
        (*pcmBufferQueue)->Enqueue(pcmBufferQueue, audioOutBuffer, dataSize);
    }
}

void *decodeVideo(void *params) {
    //创建过滤器
    const AVBitStreamFilter *pFilter;
    AVBSFContext *absCtx;
    //-filters 查看支持的过滤器集合
    pFilter = av_bsf_get_by_name("h264_mp4toannexb");

    av_bsf_alloc(pFilter, &absCtx);
    avcodec_parameters_copy(absCtx->par_in, avFormatContext->streams[videoIndex]->codecpar);
    av_bsf_init(absCtx);
    int result;

    while (startDecode) {
        AVPacket *avPacket = av_packet_alloc();

        //消费者获取packet
        videoQueue->get(avPacket);

        int size = avPacket->size;
        LOGI("video 消费者packet size:%d", size);

        //========过滤器,获取h264码流=========
        result = av_bsf_send_packet(absCtx, avPacket);
        if (result != 0) {
            LOGE("av_bsf_send_packet error code:%d", result);
            freePacket(&avPacket);
            continue;
        }
        result = av_bsf_receive_packet(absCtx, avPacket);
        while (result == 0) {
            //result为0说明成功,需要继续读取,直到不等于0,说明没有数据
            result = av_bsf_receive_packet(absCtx, avPacket);
        }

        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, 0) == JNI_OK) {
            //由于在native子线程,需要特殊处理回调java层
            jbyteArray pArray = jniEnv->NewByteArray(size);
            jniEnv->SetByteArrayRegion(pArray, 0, size,
                                       reinterpret_cast<const jbyte *>(avPacket->data));
            jniEnv->CallVoidMethod(instance, onCallData, pArray);
            jniEnv->DeleteLocalRef(pArray);
            javaVM->DetachCurrentThread();
        }

        //通知解码
        result = avcodec_send_packet(videoContext, avPacket);
        if (result != 0) {
            LOGE("video avcodec_send_packet error code:%d", result);
            freePacket(&avPacket);
            continue;
        }
        auto startTime = std::chrono::steady_clock::now();
        //创建接收帧容器
        AVFrame *videoFrame = av_frame_alloc();
        //接收解码器输出的数据
        result = avcodec_receive_frame(videoContext, videoFrame);
        if (result != 0) {
            freeFrame(&videoFrame);
            freePacket(&avPacket);
            LOGE("video avcodec_receive_frame error code:%d", result);
            continue;
        }
//        LOGI("帧数据 pts:%lld,dts:%lld,duration:%lld,pictType:%d",videoFrame->pts,videoFrame->pkt_dts,videoFrame->pkt_duration,videoFrame->pict_type);

        //将yuv数据转成rgb
        sws_scale(videoSwsContext, videoFrame->data, videoFrame->linesize, 0, videoContext->height,
                  rgbFrame->data, rgbFrame->linesize);
        //此时rgbframe.data中已经有数据了

        //windowBuffer 入参出参,
        ANativeWindow_lock(nativeWindow, &windowBuffer, nullptr);

        //转成数组
        auto *dstWindow = static_cast<uint8_t *>(windowBuffer.bits);

        for (int i = 0; i < videoContext->height; ++i) {
            //从rgbFrame->data中拷贝数据到window中
            //windowBuffer.stride * 4 === 像素*4个字节(rgba)
            memcpy(dstWindow + i * windowBuffer.stride * 4,
                   shadowedOutbuffer + i * rgbFrame->linesize[0],
                   rgbFrame->linesize[0]);

        }
        ANativeWindow_unlockAndPost(nativeWindow);

        const char *type;
        AVPictureType pictureType = videoFrame->pict_type;
        switch (pictureType) {
            case AV_PICTURE_TYPE_I: {
                type = "I帧";
                break;
            }
            case AV_PICTURE_TYPE_P: {
                type = "P帧";
                break;
            }
            case AV_PICTURE_TYPE_B: {
                type = "B帧";
                break;
            }
            default : {
                type = &"其他"[pictureType];
                break;
            }
        }


        freeFrame(&videoFrame);
        freePacket(&avPacket);
        auto endTime = std::chrono::steady_clock::now();
        auto diffMicro = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        LOGI("video 解码耗时:%f微秒 %f毫秒 帧类型:%s size:%d", diffMicro, diffMilli, type, size);
    }
    return nullptr;
}

void freePacket(AVPacket **pPacket) {
    LOGI("freePacket %p", &*pPacket);
    av_packet_free(&*pPacket);
    av_free(*pPacket);
    *pPacket = nullptr;
}

void freeFrame(AVFrame **pAvFrame) {
    LOGI("freeFrame %p", &*pAvFrame);
    av_frame_free(&*pAvFrame);
    av_free(*pAvFrame);
    *pAvFrame = nullptr;
}


int sampleConvertOpensles(int sample_rate) {
    int rate = 0;
    switch (sample_rate) {
        case 8000:
            rate = SL_SAMPLINGRATE_8;
            break;
        case 11025:
            rate = SL_SAMPLINGRATE_11_025;
            break;
        case 12000:
            rate = SL_SAMPLINGRATE_12;
            break;
        case 16000:
            rate = SL_SAMPLINGRATE_16;
            break;
        case 22050:
            rate = SL_SAMPLINGRATE_22_05;
            break;
        case 24000:
            rate = SL_SAMPLINGRATE_24;
            break;
        case 32000:
            rate = SL_SAMPLINGRATE_32;
            break;
        case 44100:
            rate = SL_SAMPLINGRATE_44_1;
            break;
        case 48000:
            rate = SL_SAMPLINGRATE_48;
            break;
        case 64000:
            rate = SL_SAMPLINGRATE_64;
            break;
        case 88200:
            rate = SL_SAMPLINGRATE_88_2;
            break;
        case 96000:
            rate = SL_SAMPLINGRATE_96;
            break;
        case 192000:
            rate = SL_SAMPLINGRATE_192;
            break;
        default:
            rate = SL_SAMPLINGRATE_44_1;

    }
    return rate;

}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_release(JNIEnv *env, jobject thiz) {
    startDecode = false;
}

int initMuxerParams(char *destPath) {
    int result;
    result = avformat_alloc_output_context2(&outFormatContext, nullptr, nullptr, destPath);
    if (result < 0) {
        LOGI("avformat_alloc_output_context2 result:%d", result);
        return -1;
    }
    AVStream *outStream = avformat_new_stream(outFormatContext, NULL);
    result = avcodec_parameters_copy(outStream->codecpar,
                                     avFormatContext->streams[videoIndex]->codecpar);

    if (result < 0) {
        LOGE("avcodec_parameters_copy result:%d", result);
        return -1;
    }

    outStream->codecpar->codec_tag = 0;

    result = avio_open(&outFormatContext->pb, destPath, AVIO_FLAG_WRITE);
    if (result < 0) {
        LOGE("avio_open result:%d", result);
        return -1;
    }

    result = avformat_write_header(outFormatContext, nullptr);
    if (result < 0) {
        LOGE("avformat_write_header result:%d", result);
        return -1;
    }
    int startTime = 3;
    int endTime = 8;
    result = av_seek_frame(avFormatContext, -1, startTime * AV_TIME_BASE, AVSEEK_FLAG_ANY);
    if (result < 0) {
        LOGE("av_seek_frame result:%d", result);
        return -1;
    }

    int64_t dtsStart;
    int64_t ptsStart;
    while (1) {
        AVStream *inStream, *outStream;
        auto avPacket = av_packet_alloc();

        int result = av_read_frame(avFormatContext, avPacket);
        if (result < 0) {
            LOGI("cutting read done!!!");
            freePacket(&avPacket);
            break;
        }
        if (avPacket->stream_index == videoIndex) {
            inStream = avFormatContext->streams[avPacket->stream_index];
            outStream = outFormatContext->streams[avPacket->stream_index];

            auto timeBase = av_q2d(inStream->time_base);
            LOGI("cutting timebase:%f,pts:%lld result:%f", timeBase, avPacket->pts,
                 timeBase * avPacket->pts);

            if (timeBase * avPacket->pts > endTime) {
                freePacket(&avPacket);
                break;
            }

            if (dtsStart == 0) {
                dtsStart = avPacket->dts;
            }
            if (ptsStart == 0) {
                ptsStart = avPacket->pts;
            }

            // 时间基转换
            int rnd = AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX;
            avPacket->pts = av_rescale_q_rnd(avPacket->pts - ptsStart, inStream->time_base,
                                             outStream->time_base,
                                             (AVRounding) rnd);

            avPacket->dts = av_rescale_q_rnd(avPacket->dts - dtsStart, inStream->time_base,
                                             outStream->time_base,
                                             (AVRounding) rnd);

            LOGI("cutting av_rescale_q_rnd pts:%lld,dts:%lld,fps:%f", avPacket->pts, avPacket->pts,
                 av_q2d(outStream->avg_frame_rate));
            if (avPacket->pts < 0) {
                avPacket->pts = 0;
            }
            if (avPacket->dts < 0) {
                avPacket->dts = 0;
            }

            if (avPacket->pts < avPacket->dts) {
                //处理异常数据,pts必大于dts
                continue;
            }

            avPacket->duration = av_rescale_q(avPacket->duration, inStream->time_base,
                                              outStream->time_base);
            avPacket->pos = -1;

            result = av_interleaved_write_frame(outFormatContext, avPacket);
            if (result < 0) {
                LOGE("av_interleaved_write_frame result:%d", result);
                break;
            }
            freePacket(&avPacket);
        }

    }
    LOGI("cutting done!!!");
    av_write_trailer(outFormatContext);
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_cutting(JNIEnv *env, jobject thiz, jstring dest_path_) {
    startDecode = false;
    char *destPath = const_cast<char *>(env->GetStringUTFChars(dest_path_, 0));
    LOGI("cutting path:%s", destPath);
    int result = initMuxerParams(destPath);
    if (result != -1) {

    }

    env->ReleaseStringUTFChars(dest_path_, destPath);
}
