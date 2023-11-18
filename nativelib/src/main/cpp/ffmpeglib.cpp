#include <jni.h>
#include "loghelper.h"
#include "Queue.h"
#include "FFVideoReader.h"
#include <pthread.h>
#include <unistd.h>

#include <android/native_window_jni.h>
#include <string>

#include <list>
#include <set>

using namespace std;

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "libavcodec/jni.h"
#include "libavcodec/mediacodec.h"
#include "libavcodec/bsf.h"
}

_JavaVM *javaVM = nullptr;
jobject instance = nullptr;
jmethodID onCallData = nullptr;
jmethodID onCallCurrTime = nullptr;
jclass jclazz = nullptr;
char *path = nullptr;
//是否开启硬解
int openHwCoder = 1;
//解码线程数
int decoderThreadCount = 0;
//比例
int scale = 1;
extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jint result = -1;
    javaVM = vm;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return result;
    }
    if (openHwCoder) {
        // 设置JavaVM，否则无法进行硬解码
        av_jni_set_java_vm(vm, nullptr);
    }
    LOGI("JNI_OnLoad %p", &javaVM);

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

void initAudio();

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context);

void startLooprtDecode();

int sampleConvertOpensles(int sample_rate);

int initMuxerParams(char *destPath);

void writeH264Content(AVBSFContext **absCtx, AVPacket *pPacket);

void showFrameToWindow(AVFrame **pFrame);

AVPixelFormat hw_pix_fmt;

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    LOGE("Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

bool startDecode = false;
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_nativelib_FFMpegPlay_play(JNIEnv *env, jobject thiz, jstring url_,
                                           jobject surface) {
    instance = env->NewGlobalRef(thiz);
    jclazz = env->GetObjectClass(instance);
    onCallData = env->GetMethodID(jclazz, "onCallData", "([B)V");
    onCallCurrTime = env->GetMethodID(jclazz, "onCallCurrTime", "(F)V");

    path = const_cast<char *>(env->GetStringUTFChars(url_, 0));

    LOGI("play url:%s,是否开启硬解:%d", path, openHwCoder);
    //注册所有组件
//    av_register_all();
    if (path[0] == 'h') {
        LOGI("play init network");
        //初始化网络模块
        avformat_network_init();
    }
    int result;
    //分配 avFormatContext
    avFormatContext = avformat_alloc_context();
    //打开文件输入流
    result = avformat_open_input(&avFormatContext, path, nullptr, nullptr);
    if (result < 0) {
        LOGE("avformat_open_input result:%d", result);
        env->ReleaseStringUTFChars(url_, path);
        return false;
    }

    //提取输入文件中的数据流信息
    result = avformat_find_stream_info(avFormatContext, nullptr);

    if (result < 0) {
        LOGE("avformat_find_stream_info result:%d", result);
        env->ReleaseStringUTFChars(url_, path);
        return false;
    }
    int fps = 0;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        AVStream *pStream = avFormatContext->streams[i];
        AVCodecParameters *codecpar = pStream->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //视频流
            videoIndex = i;
            //查找对应的编解码器,传入获取到的id
            const AVCodec *avCodec = nullptr;
            if (openHwCoder) {
                avCodec = avcodec_find_decoder_by_name("h264_mediacodec");
                if (nullptr == avCodec) {
                    LOGE("video 没有找到硬解码器h264_mediacodec");
                } else {
                    // 配置硬解码器
                    int i;
                    for (i = 0;; i++) {
                        const AVCodecHWConfig *config = avcodec_get_hw_config(avCodec, i);
                        if (nullptr == config) {
                            LOGE("video 获取硬解码配置失败");
                            return false;
                        }
                        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                            config->device_type == AV_HWDEVICE_TYPE_MEDIACODEC) {
                            hw_pix_fmt = config->pix_fmt;
                            LOGI("video 硬件解码器配置成功 format:%s %d",
                                 av_get_pix_fmt_name((AVPixelFormat) hw_pix_fmt), hw_pix_fmt);
                            break;
                        }
                    }
                }
            } else {
                avCodec = avcodec_find_decoder(codecpar->codec_id);
            }

            //分配解码器分配上下文
            videoContext = avcodec_alloc_context3(avCodec);
            if (decoderThreadCount) {
                videoContext->thread_count = decoderThreadCount;
            }
            //将数据流相关的编解码参数来填充编解码器上下文
            avcodec_parameters_to_context(videoContext, codecpar);

            if (openHwCoder) {
                AVBufferRef *mHwDeviceCtx = nullptr;
                if (av_hwdevice_ctx_create(&mHwDeviceCtx, AV_HWDEVICE_TYPE_MEDIACODEC, nullptr,
                                           nullptr,
                                           0) < 0) {
                    LOGE("video av_hwdevice_ctx_create false");
                    return false;
                }
                videoContext->hw_device_ctx = av_buffer_ref(mHwDeviceCtx);
                videoContext->get_format = get_hw_format;
                //关联mediacontext
                AVMediaCodecContext *mediaCodecContext = av_mediacodec_alloc_context();
                result = av_mediacodec_default_init(videoContext, mediaCodecContext, surface);
                if (result != 0) {
                    LOGE("video av_mediacodec_default_init %d", result);
                    return false;
                }
            }

            //打开解码器
            result = avcodec_open2(videoContext, avCodec, 0);
            if (result < 0) {
                LOGE("video avcodec_open2 false result:%d", result);
                return false;
            }

            fps = av_q2d(pStream->avg_frame_rate);
            videoSleep = 1000 / fps;
            LOGI("video 视频流index:%d,width:%d,height:%d,fps:%d,duration:%lld秒,codecName:%s,codec_id:%d,thread_count:%d",
                 i, codecpar->width, codecpar->height, fps,
                 avFormatContext->duration / AV_TIME_BASE, avCodec->name, codecpar->codec_id,
                 videoContext->thread_count);

        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频流
            audioIndex = i;
            //查找对应的编解码器,传入获取到的id
            const AVCodec *avCodec = avcodec_find_decoder(codecpar->codec_id);
            //分配解码器分配上下文
            audioContext = avcodec_alloc_context3(avCodec);
            //将数据流相关的编解码参数来填充编解码器上下文
            result = avcodec_parameters_to_context(audioContext, codecpar);
            if (result != 0) {
                LOGE("audio avcodec_parameters_to_context %d", result);
            }
            //打开解码器
            result = avcodec_open2(audioContext, avCodec, 0);
            if (result != 0) {
                LOGE("audio avcodec_open2 %d", result);
            }
            LOGI("audio 音频流index:%d,sample_rate:%d,channels:%d,frame_size:%d,codecId:%d,name:%p",
                 i,
                 codecpar->sample_rate,
                 codecpar->channels, codecpar->frame_size, codecpar->codec_id, &avCodec->name);
        } else {
            LOGI("其他流index:%d,codec_type:%d", i, codecpar->codec_type);
        }
    }
    //回调给应用层
    jmethodID jmethodId = env->GetMethodID(jclazz, "onConfig", "(IIJI)V");
    env->CallVoidMethod(thiz, jmethodId, videoContext->width, videoContext->height,
                        avFormatContext->duration / AV_TIME_BASE, fps);
    //获取缓冲window
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    //修改缓冲区格式和大小,对应视频格式和大小
    ANativeWindow_setBuffersGeometry(nativeWindow, videoContext->width, videoContext->height,
                                     WINDOW_FORMAT_RGBA_8888);

    startDecode = true;

    initVideo();
    initAudio();
    startLooprtDecode();

//    env->ReleaseStringUTFChars(url_, path);
    return false;
}

bool initAudioSuccess() {
    return &(avcodec_find_decoder(avFormatContext->streams[audioIndex]->codecpar->codec_id)->name)
           != nullptr;
}

void initAudio() {
    if (!initAudioSuccess()) {
        LOGE("initAudio audioContext not init");
        return;
    }
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
    int dstWidth = videoContext->width / scale;
    int dstHeight = videoContext->height / scale;
    //初始存放目标数据的化容器
    rgbFrame = av_frame_alloc();
    //获取容器大小 格式,宽高
    int buffSize = av_image_get_buffer_size(format, dstWidth, dstHeight, 1);
    //分配内存空间给容器
    shadowedOutbuffer = static_cast<uint8_t *>(av_malloc(buffSize * sizeof(uint8_t)));
    //将rgbFrame->data指针指向outbuffer,等同于rgbFrame->data=shadowedOutbuffer
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, shadowedOutbuffer, format,
                         dstWidth, dstHeight, 1);

    //初始化转换上下文,原视频宽高+format,目标视频宽高+format
    videoSwsContext = sws_getContext(videoContext->width, videoContext->height,
                                     videoContext->pix_fmt,
                                     dstWidth, dstHeight, format,
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
    LOGI("decodePacket 读取数据包");

    auto startTime = std::chrono::steady_clock::now();

    while (startDecode) {
        LOGI("decodePacket start read");
        //创建packet接收
        AVPacket *avPacket = av_packet_alloc();
        //读取数据
        int result = av_read_frame(avFormatContext, avPacket);
        if (result < 0) {
            LOGI("decodePacket read done!!!");
            videoQueue->setDone();
            av_packet_unref(avPacket);
            //结尾
            break;
        }
        if (avPacket->stream_index == videoIndex) {
            //延迟帧率ms
//            usleep(videoSleep * 1000);
            LOGI("decodePacket 视频包大小:%d", avPacket->size);
            videoQueue->push(avPacket);
        } else if (avPacket->stream_index == audioIndex) {
            if (initAudioSuccess()) {
                LOGI("decodePacket 音频包大小:%d", avPacket->size);
                audioQueue->push(avPacket);
            }
        }
    }
    auto endTime = std::chrono::steady_clock::now();
    auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    LOGI("decodePacket 耗时:%f毫秒", diffMilli);
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
            av_packet_unref(avPacket);
            continue;
        }
        LOGI("audio start receive frame");
        auto startTime = std::chrono::steady_clock::now();
        //创建接收帧容器
        AVFrame *audioFrame = av_frame_alloc();
        //接收解码器输出的数据
        result = avcodec_receive_frame(audioContext, audioFrame);
        if (result != 0) {
            av_frame_unref(audioFrame);
            av_packet_unref(avPacket);
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
            av_frame_unref(audioFrame);
            av_packet_unref(avPacket);
            LOGE("audio swr_ctx init fail %p", &swr_ctx);
            continue;
        }

        //开始转换
        int nb = swr_convert(swr_ctx, &audioOutBuffer, audioFrame->nb_samples,
                             (const uint8_t **) audioFrame->data, audioFrame->nb_samples);
        if (nb < 0) {
            av_frame_unref(audioFrame);
            av_packet_unref(avPacket);
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
        auto diff = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        LOGI("audio 解码耗时:%f毫秒 sample_rate:%d,channels:%d,format:%d,", diff,
             audioFrame->sample_rate, audioFrame->channels, audioFrame->format);

        av_frame_unref(audioFrame);
        av_packet_unref(avPacket);
        swr_free(&swr_ctx);
        break;
    }
    if (dataSize > 0) {
        LOGI("audio pcmBufferQueue Enqueue");
        (*pcmBufferQueue)->Enqueue(pcmBufferQueue, audioOutBuffer, dataSize);
    }
}

void *decodeVideo(void *params) {
    AVBSFContext *absCtx;
    const AVBitStreamFilter *pFilter;
    //创建过滤器
    //-filters 查看支持的过滤器集合
    pFilter = av_bsf_get_by_name("h264_mp4toannexb");
    av_bsf_alloc(pFilter, &absCtx);
    avcodec_parameters_copy(absCtx->par_in, avFormatContext->streams[videoIndex]->codecpar);
    av_bsf_init(absCtx);

    auto timeBase = av_q2d(avFormatContext->streams[videoIndex]->time_base);
    start:
    while (startDecode) {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        AVPacket *avPacket = av_packet_alloc();
        int result;
        videoQueue->get(avPacket);

        int nalType = avPacket->data[4] & 0x1F;
        LOGI("video videoQueue get packet size:%d pts:%lld dts:%lld flags:%d nalType:%d",
             avPacket->size,
             avPacket->pts, avPacket->dts, avPacket->flags, nalType);

        writeH264Content(&absCtx, avPacket);
        /**发送数据包,通知解码
         * avcodec_send_packet
         * 0:通知解码成功
         * AVERROR(EAGAIN):缓冲区已满,需要调用avcodec_receive_frame获取解码后的数据,当前数据包不能丢弃,否则会丢帧
         * AVERROR_EOF:结束,但是可以继续从缓冲区获取数据
         *
         * 获取解码后的数据
         * avcodec_receive_frame
         * 0:获取解码数据成功
         * AVERROR(EAGAIN):缓冲区已空,需要发送新的数据包,此情况判断如果avcodec_send_packet==AVERROR(EAGAIN),继续发送当前数据包,并且需要判断返回值,对应判断是否需要继续获取解码数据
         * AVERROR_EOF:缓冲区刷新完成,后续不再有解码数据
         *
         * 如果无发送包,但是缓冲区还有数据,可以设置avPacket->data=null,avPacket->size=0;发送后再获取解码数据
         */
        result = avcodec_send_packet(videoContext, avPacket);
        //是否需要重新send
        int needReSend = 0;
        if (result == 0) {
            av_packet_unref(avPacket);
            LOGI("video avcodec_send_packet success");
        } else if (result == AVERROR(EAGAIN)) {
            needReSend = 1;
            //缓冲区已满，要从内部缓冲区读取解码后的音视频帧
            LOGI("video avcodec_send_packet AVERROR(EAGAIN)");
        } else if (result == AVERROR_EOF) {
            av_packet_unref(avPacket);
            // 数据包送入结束不再送入,但是可以继续可以从内部缓冲区读取解码后的音视频帧
            LOGI("video avcodec_send_packet AVERROR_EOF");
        } else {
            LOGE("video avcodec_send_packet error code:%d", result);
            continue;
        }
        do {
            frame:
            LOGI("video start receive frame");
            AVFrame *avFrame = av_frame_alloc();
            // AVERROR(EAGAIN):缓冲区已空，没有帧输出，需要更多的送入数据包
            // AVERROR_EOF:解码缓冲区已经刷新完成,后续不再有音视频帧输出
            result = avcodec_receive_frame(videoContext, avFrame);

            if (result == AVERROR(EAGAIN)) {
                LOGI("video avcodec_receive_frame AVERROR(EAGAIN)");
                av_frame_unref(avFrame);
                if (needReSend) {
                    int tResult = avcodec_send_packet(videoContext, avPacket);
                    LOGI("video avcodec_send_packet while %d", tResult);
                    if (tResult == 0 || tResult == AVERROR(EAGAIN)) {
                        if (tResult == 0) {
                            av_packet_unref(avPacket);
                            needReSend = 0;
                        }
                        goto frame;
                    } else {
                        av_packet_unref(avPacket);
                        goto start;
                    }
                }
            } else if (result == AVERROR_EOF) {
                av_frame_unref(avFrame);
                LOGI("video avcodec_receive_frame AVERROR_EOF");
                goto start;
            } else if (result == 0) {
                float currTime = timeBase * avFrame->pts;
                LOGI("video 帧数据 width:%d,height:%d,frameFormat:%s %d,flags:%d,key_frame:%d,currTime:%f",
                     avFrame->width, avFrame->height,
                     av_get_pix_fmt_name((AVPixelFormat) avFrame->format), avFrame->format,
                     avFrame->flags, avFrame->key_frame, currTime);
                JNIEnv *jniEnv;
                if (javaVM->AttachCurrentThread(&jniEnv, nullptr) == JNI_OK) {
                    //由于在native子线程,需要特殊处理回调java层
                    jniEnv->CallVoidMethod(instance, onCallCurrTime, currTime);
                    if (!openHwCoder) {
                        //开启硬解后vm给ffmpeg管理,如果detach会崩溃
                        javaVM->DetachCurrentThread();
                    }
                }
                const char *type;
                AVPictureType pictureType = avFrame->pict_type;
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
                        type = "其他";
                        break;
                    }
                }
                endTime = std::chrono::steady_clock::now();
                diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();

                LOGI("video 解码耗时:%f毫秒 帧类型:%s-%d", diffMilli, type, pictureType);
                showFrameToWindow(&avFrame);
                av_frame_unref(avFrame);
            } else {
                av_frame_unref(avFrame);
                LOGE("video avcodec_receive_frame error code:%d", result);
                if (needReSend && startDecode) {
                    goto frame;
                }
            }
        } while (result == 0);
    }
    LOGI("parse all frame done!!!");
    return nullptr;
}

void showFrameToWindow(AVFrame **pFrame) {
    AVFrame *avFrame = nullptr;
    //硬解,并且配置直接关联surface,format为mediacoder
    bool hwFmt = openHwCoder && hw_pix_fmt && (*pFrame)->format == hw_pix_fmt;
    LOGI("video showFrameToWindow hwFmt:%d", hwFmt);
    if (hwFmt) {
        auto startTime = std::chrono::steady_clock::now();
        //直接渲染到surface
        int result = av_mediacodec_release_buffer((AVMediaCodecBuffer * )(*pFrame)->data[3], 1);
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        LOGI("video av_mediacodec_release_buffer 耗时:%f毫秒 result:%d", diffMilli, result);
//        avFrame = av_frame_alloc();
//        result = av_hwframe_transfer_data(avFrame, (*pFrame), 0);
//        LOGI("video av_hwframe_map %d", result);
        return;
    } else {
        avFrame = *pFrame;
    }

    auto startTime = std::chrono::steady_clock::now();
    //将yuv数据转成rgb
    sws_scale(videoSwsContext, avFrame->data, avFrame->linesize, 0,
              videoContext->height,
              rgbFrame->data, rgbFrame->linesize);
    auto endTime = std::chrono::steady_clock::now();
    auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    LOGI("video sws_scale yuv->rgb 耗时:%f毫秒", diffMilli);
    //此时rgbframe.data中已经有数据了

    //windowBuffer 入参出参,
    ANativeWindow_lock(nativeWindow, &windowBuffer, nullptr);

    //转成数组
    auto *dstWindow = static_cast<uint8_t *>(windowBuffer.bits);

    startTime = std::chrono::steady_clock::now();
    for (int i = 0; i < videoContext->height / scale; ++i) {
        //从rgbFrame->data中拷贝数据到window中
        //windowBuffer.stride * 4 === 像素*4个字节(rgba)
        memcpy(dstWindow + i * windowBuffer.stride * 4,
               shadowedOutbuffer + i * rgbFrame->linesize[0],
               rgbFrame->linesize[0]);
    }
    endTime = std::chrono::steady_clock::now();
    diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    LOGI("video rgb->显示 耗时:%f毫秒", diffMilli);
    ANativeWindow_unlockAndPost(nativeWindow);
    if (hwFmt) {
        av_frame_unref(avFrame);
    }
}

void writeH264Content(AVBSFContext **absCtx, AVPacket *avPacket) {
    AVPacket *tempPacket = av_packet_alloc();
    av_packet_ref(tempPacket, avPacket);
    int size = tempPacket->size;
    int result = av_bsf_send_packet(*absCtx, tempPacket);
    if (result != 0) {
        LOGE("video writeH264Content av_bsf_send_packet error code:%d", result);
        av_packet_unref(tempPacket);
        return;
    }

    do {
        //result为0说明成功,需要继续读取,直到不等于0,说明没有数据
        result = av_bsf_receive_packet(*absCtx, tempPacket);
    } while (result == 0);
    JNIEnv *jniEnv;
    if (javaVM->AttachCurrentThread(&jniEnv, 0) == JNI_OK) {
        //由于在native子线程,需要特殊处理回调java层
        jbyteArray pArray = jniEnv->NewByteArray(size);
        jniEnv->SetByteArrayRegion(pArray, 0, size,
                                   reinterpret_cast<const jbyte *>(tempPacket->data));
        jniEnv->CallVoidMethod(instance, onCallData, pArray);
        jniEnv->DeleteLocalRef(pArray);
        if (!openHwCoder) {
            javaVM->DetachCurrentThread();
        }
    }
    av_packet_unref(tempPacket);
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

void release() {
    startDecode = false;
    videoQueue->clearAvpacket();
    audioQueue->clearAvpacket();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_release(JNIEnv *env, jobject thiz) {
    release();
}

int initMuxerParams(char *destPath) {
    int result;
    result = avformat_alloc_output_context2(&outFormatContext, nullptr, nullptr, destPath);
    if (result < 0) {
        LOGI("cutting avformat_alloc_output_context2 result:%d", result);
        return -1;
    }

    AVStream *inStream = avFormatContext->streams[videoIndex];
    AVStream *outStream = avformat_new_stream(outFormatContext, NULL);
//    outStream->avg_frame_rate = av_make_q(15, 1);
//    outStream->time_base = av_inv_q(outStream->avg_frame_rate);
//    videoContext->framerate = outStream->avg_frame_rate;

    outStream->time_base = inStream->time_base;

    result = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    if (result < 0) {
        LOGE("cutting avcodec_parameters_copy result:%d", result);
        return -1;
    }

    result = avio_open(&outFormatContext->pb, destPath, AVIO_FLAG_WRITE);
    if (result < 0) {
        LOGE("cutting avio_open result:%d", result);
        return -1;
    }

    //设置codec_tag=0,内部会找对应封装格式的tag
    outStream->codecpar->codec_tag = 0;
    result = avformat_write_header(outFormatContext, nullptr);
    if (result < 0) {
        LOGE("cutting avformat_write_header result:%d", result);
        return -1;
    }

    int startTime = 3;
    int endTime = 13;
    auto timeBase = av_q2d(inStream->time_base);
    //s=pts*timebase
    long long startPts = startTime / timeBase;
    long long endtPts = endTime / timeBase;
    LOGI("cutting inStream startPts:%lld timeBase:%f", startPts, timeBase);

    result = av_seek_frame(avFormatContext, -1, startPts,
                           AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
    if (result < 0) {
        LOGE("cutting av_seek_frame result:%d", result);
        return -1;
    }
    for (;;) {
        auto avPacket = av_packet_alloc();

        result = av_read_frame(avFormatContext, avPacket);
        if (result != 0) {
            LOGI("cutting read done!!!");
            av_packet_unref(avPacket);
            break;
        }
        if (avPacket->stream_index != videoIndex) {
            av_packet_unref(avPacket);
            continue;
        }
        LOGI("===========start cutting video=============");

        if (avPacket->pts > endtPts) {
            LOGE("cutting pts>endTime");
            av_packet_unref(avPacket);
            break;
        }
        int filterB = 0;
        if (filterB) {
            videoContext->skip_frame = AVDISCARD_BIDIR;
            //通知解码
            result = avcodec_send_packet(videoContext, avPacket);
            if (result != 0) {
                LOGE("cutting video avcodec_send_packet error code:%d", result);
                av_packet_unref(avPacket);
                continue;
            }
            //创建接收帧容器
            AVFrame *videoFrame = av_frame_alloc();
            //接收解码器输出的数据
            result = avcodec_receive_frame(videoContext, videoFrame);
            if (result != 0) {
                av_frame_unref(videoFrame);
                av_packet_unref(avPacket);
                LOGE("cutting video avcodec_receive_frame error code:%d", result);
                continue;
            }
            const char *type;
            int filter = 0;
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
            LOGI("cutting %s %p %p", type, videoFrame->data, avPacket->data);
            av_frame_unref(videoFrame);
        }

        LOGI("cutting pts avPacket->pts:(%f) startPts(%f)",
             timeBase * avPacket->pts, timeBase * startPts);
        LOGI("cutting dts avPacket->dts:(%f) startPts(%f)",
             timeBase * avPacket->dts, timeBase * startPts);

        // 时间基转换
        int rnd = AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX;
        avPacket->pts = av_rescale_q_rnd(avPacket->pts - startPts, inStream->time_base,
                                         outStream->time_base,
                                         (AVRounding) rnd);

        avPacket->dts = av_rescale_q_rnd(avPacket->dts - startPts, inStream->time_base,
                                         outStream->time_base,
                                         (AVRounding) rnd);

        avPacket->duration = av_rescale_q(avPacket->duration, inStream->time_base,
                                          outStream->time_base);
        avPacket->pos = -1;

        LOGI("cutting av_rescale_q_rnd pts:%f,dts:%f,fps:%f,duration:%f,packet flags:%d",
             timeBase * avPacket->pts, timeBase * avPacket->dts,
             av_q2d(outStream->avg_frame_rate), timeBase * avPacket->duration, avPacket->flags);

        if (avPacket->pts < avPacket->dts) {
            LOGE("cutting pts<dts");
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
long long startPts;
list<long long> sendDoneSet;
extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_seekTo(JNIEnv *env, jobject thiz, jfloat _duration) {
    release();
    jfloat duration = _duration;
    auto inStream = avFormatContext->streams[videoIndex];
    auto timeBase = av_q2d(inStream->time_base);
    //s=pts*timebase
    long long tStartPts = duration / timeBase;
    if (tStartPts < startPts) {
        avcodec_flush_buffers(videoContext);
        sendDoneSet.clear();
        LOGI("seek video back avcodec_flush_buffers");
    } else if (tStartPts == 0) {
        avcodec_flush_buffers(videoContext);
        sendDoneSet.clear();
        LOGI("seek video 0 avcodec_flush_buffers");
        //int av_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp,
    }
    startPts = tStartPts;
    int result = 0;

    //stream_index为-1,timestamp为时间(us)
    //stream_index为视频index,timestamp为pts
//    result = av_seek_frame(avFormatContext, videoIndex, startPts,
//                            AVSEEK_FLAG_ANY
//    );

    /** AVSEEK_FLAG_BACKWARD:是seek到请求的timestamp之前最近的关键帧
AVSEEK_FLAG_BYTE: 是基于字节位置的查找
AVSEEK_FLAG_ANY: 是可以seek到任意帧，注意不一定是关键帧，因此使用时可能会导致花屏
AVSEEK_FLAG_FRAME:是基于帧数量快进

     AVSEEK_FLAG_ANY+AVSEEK_FLAG_FRAME只能ip
     */
    result = avformat_seek_file(avFormatContext, videoIndex, INT64_MIN, startPts, INT64_MAX,
                                AVSEEK_FLAG_ANY
    );


    LOGI("seek result:%d duration:%f startPts:%lld", result, duration, startPts);
    start:
    while (true) {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = std::chrono::steady_clock::now();
        auto diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        int result;
        //创建packet接收
        AVPacket *avPacket = av_packet_alloc();
        //读取数据
        int isVideo = 0;
        do {
            result = av_read_frame(avFormatContext, avPacket);
            if (result < 0) {
                LOGI("seek decodePacket read done!!!");
                av_packet_unref(avPacket);
                //结尾
                return;
            }
            if (avPacket->stream_index == videoIndex) {
                LOGI("seek decodePacket 视频包大小:%d pts:%lld", avPacket->size, avPacket->pts);
            }
            isVideo = avPacket->stream_index == videoIndex;
            if (!isVideo) {
                av_packet_unref(avPacket);
            }
        } while (!isVideo);
//        if (isVideo) {
//            goto done;
//        }
        int nalType = avPacket->data[4] & 0x1F;
        LOGI("seek video packet size:%d flags:%d pts:%lld dts:%lld nalType:%d", avPacket->size,
             avPacket->flags, avPacket->pts, avPacket->dts, nalType);

        auto pts = avPacket->pts;
        auto findResult = std::find(sendDoneSet.begin(), sendDoneSet.end(), pts);
        if (findResult != sendDoneSet.end()) {
            av_packet_unref(avPacket);
            LOGI("seek findResult pts:%lld", pts);
            continue;
        }

        /**发送数据包,通知解码
         * avcodec_send_packet
         * 0:通知解码成功
         * AVERROR(EAGAIN):缓冲区已满,需要调用avcodec_receive_frame获取解码后的数据,当前数据包不能丢弃,否则会丢帧
         * AVERROR_EOF:结束,但是可以继续从缓冲区获取数据
         *
         * 获取解码后的数据
         * avcodec_receive_frame
         * 0:获取解码数据成功
         * AVERROR(EAGAIN):缓冲区已空,需要发送新的数据包,此情况判断如果avcodec_send_packet==AVERROR(EAGAIN),继续发送当前数据包,并且需要判断返回值,对应判断是否需要继续获取解码数据
         * AVERROR_EOF:缓冲区刷新完成,后续不再有解码数据
         *
         * 如果无发送包,但是缓冲区还有数据,可以设置avPacket->data=null,avPacket->size=0;发送后再获取解码数据
         */
        result = avcodec_send_packet(videoContext, avPacket);

        //是否需要重新send
        int needReSend = 0;
        if (result == 0) {
            LOGI("seek video avcodec_send_packet success");
            sendDoneSet.push_front(pts);
            if (avPacket->pts > startPts) {
                av_packet_unref(avPacket);
                LOGI("seek video avPacket->pts>startPts");
//            avcodec_flush_buffers(videoContext);
                goto done;
            }
            av_packet_unref(avPacket);
        } else if (result == AVERROR(EAGAIN)) {
            needReSend = 1;
            //缓冲区已满，要从内部缓冲区读取解码后的音视频帧
            LOGI("seek video avcodec_send_packet AVERROR(EAGAIN)");
        } else if (result == AVERROR_EOF) {
            av_packet_unref(avPacket);
            // 数据包送入结束不再送入,但是可以继续可以从内部缓冲区读取解码后的音视频帧
            LOGI("seek video avcodec_send_packet AVERROR_EOF");
        } else {
            LOGE("seek video avcodec_send_packet error code:%d", result);
            continue;
        }

        do {
            frame:
            LOGI("seek video start receive frame");
            AVFrame *avFrame = av_frame_alloc();
            // AVERROR(EAGAIN):缓冲区已空，没有帧输出，需要更多的送入数据包
            // AVERROR_EOF:解码缓冲区已经刷新完成,后续不再有音视频帧输出
            result = avcodec_receive_frame(videoContext, avFrame);

            if (result == AVERROR(EAGAIN)) {
                LOGI("seek video avcodec_receive_frame AVERROR(EAGAIN)");
                av_frame_unref(avFrame);
                if (needReSend) {
                    int tResult = avcodec_send_packet(videoContext, avPacket);
                    LOGI("seek video avcodec_send_packet needReSend %d", tResult);
                    if (tResult == 0 || tResult == AVERROR(EAGAIN)) {
                        if (tResult == 0) {
                            sendDoneSet.push_front(pts);
                            av_packet_unref(avPacket);
                            needReSend = 0;
                        }
                        goto frame;
                    } else {
                        av_packet_unref(avPacket);
                        goto done;
                    }
                } else {
                    LOGI("seek video re read frame");
//                    avPacket->data = nullptr;
//                    avPacket->size = 0;
//                    int tReuslt = avcodec_send_packet(videoContext, avPacket);
//                    LOGI("seek video avcodec_send_packet null data %d", tReuslt);
                    goto start;
                }
            } else if (result == AVERROR_EOF) {
                av_frame_unref(avFrame);

//                avcodec_flush_buffers(videoContext);
                LOGI("seek video avcodec_receive_frame AVERROR_EOF");
                goto start;
            } else if (result == 0) {
                LOGI("seek video 帧数据 width:%d,height:%d,frameFormat:%s %d,flags:%d,key_frame:%d,pts:%lld,frameTime:%f",
                     avFrame->width, avFrame->height,
                     av_get_pix_fmt_name((AVPixelFormat) avFrame->format), avFrame->format,
                     avFrame->flags, avFrame->key_frame, avFrame->pts, timeBase * avFrame->pts);


                const char *type;
                AVPictureType pictureType = avFrame->pict_type;
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
                        type = "其他";
                        break;
                    }
                }
                endTime = std::chrono::steady_clock::now();
                diffMilli = std::chrono::duration<double, std::milli>(endTime - startTime).count();

                LOGI("seek video 解码耗时:%f毫秒 帧类型:%s-%d", diffMilli, type, pictureType);
                showFrameToWindow(&avFrame);

                av_frame_unref(avFrame);
            } else {
                av_frame_unref(avFrame);
                LOGE("video avcodec_receive_frame error code:%d", result);
                if (needReSend) {
                    goto frame;
                }
            }
        } while (result == 0);
    }
    done:
    LOGI("seek frame result:%d", result);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_resume(JNIEnv *env, jobject thiz) {
    startDecode = true;
    startLooprtDecode();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_pause(JNIEnv *env, jobject thiz) {
    release();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_getVideoFrames(JNIEnv *env, jobject thiz,
                                                     jint width, jint height, jboolean precise,
                                                     jobject callback) {
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
    std::string s_path = path;
    LOGI("getVideoFrames path:%s", path)
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