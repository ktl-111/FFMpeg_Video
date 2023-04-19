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
}
AVFormatContext *avFormatContext;
AVCodecContext *videoContext;
//等同于rgbframe.data,两个指针指的是同一个位置,由于后面copy时需要移动指针,但是rgbFrame.data需要用来存数据,不允许移动指针
//所以定义个影子buffer
uint8_t *shadowedOutbuffer;
AVFrame *rgbFrame;
SwsContext *swsContext;
Queue *videoQueue;
int videoIndex;
int audioIndex;
enum AVPixelFormat format = AV_PIX_FMT_RGBA;

ANativeWindow *nativeWindow;
ANativeWindow_Buffer windowBuffer;

void startLooperVideo();

void *decodePacket(void *params);

void *decodeVideo(void *params);

void freePacket(AVPacket **pPacket);

void freeFrame(AVFrame **pAvFrame);

bool startDecode = false;
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_nativelib_FFMpegPlay_play(JNIEnv *env, jobject thiz, jstring url_,
                                           jobject surface) {
    char *url = const_cast<char *>(env->GetStringUTFChars(url_, 0));

    LOGI("play url:%s", url);
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
        AVCodecParameters *codecpar = avFormatContext->streams[i]->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            //判断流type是否为视频流
            LOGI("视频流index:%d,width:%d,height:%d", i, codecpar->width, codecpar->height);
            //查找对应的编解码器,传入获取到的id
            AVCodec *avCodec = avcodec_find_decoder(codecpar->codec_id);
            //分配解码器分配上下文
            videoContext = avcodec_alloc_context3(avCodec);
            //将数据流相关的编解码参数来填充编解码器上下文
            avcodec_parameters_to_context(videoContext, codecpar);
            //打开解码器
            avcodec_open2(videoContext, avCodec, 0);
        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
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
    startLooperVideo();
    env->ReleaseStringUTFChars(url_, url);
    return false;
}

void startLooperVideo() {
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
    swsContext = sws_getContext(videoContext->width, videoContext->height, videoContext->pix_fmt,
                                videoContext->width, videoContext->height, format,
                                SWS_BICUBIC, nullptr, nullptr, nullptr);

    //创建生产消费者,loop线程
    videoQueue = new Queue();
    pthread_t thread_decode;
    pthread_t thread_vidio;
    pthread_create(&thread_decode, nullptr, decodePacket, nullptr);
    pthread_create(&thread_vidio, nullptr, decodeVideo, nullptr);
}

void *decodePacket(void *params) {
    LOGI("读取数据包");
    while (startDecode) {
//        if (videoQueue->size() > 100) {
//            usleep(100 * 1000);
//        }

        LOGI("start read");
        //创建packet接收
        AVPacket *avPacket = av_packet_alloc();
        //读取数据
        int result = av_read_frame(avFormatContext, avPacket);
        if (result < 0) {
            LOGI("read done!!!");
            //结尾
            break;
        }
        if (avPacket->stream_index == videoIndex) {
            LOGI("视频包大小:%d", avPacket->size);
            videoQueue->push(avPacket);
        } else if (avPacket->stream_index == audioIndex) {
            LOGI("音频包大小:%d", avPacket->size);
        }
    }
    return nullptr;
}

void *decodeVideo(void *params) {

    while (startDecode) {
        AVPacket *avPacket = av_packet_alloc();
        //消费者获取packet
        videoQueue->get(avPacket);
        LOGI("消费者packet size:%d", avPacket->size);
        //通知解码
        int result = avcodec_send_packet(videoContext, avPacket);
        if (result != 0) {
            LOGI("avcodec_send_packet error code:%d", result);
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
            LOGI("avcodec_receive_frame error code:%d", result);
            continue;
        }
//        LOGI("帧数据 pts:%lld,dts:%lld,duration:%lld,pictType:%d",videoFrame->pts,videoFrame->pkt_dts,videoFrame->pkt_duration,videoFrame->pict_type);
        //将yuv数据转成rgb
        sws_scale(swsContext, videoFrame->data, videoFrame->linesize, 0, videoContext->height,
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
        auto diff = std::chrono::duration<double, std::micro>(endTime - startTime).count();

        LOGI("解码耗时 by micro:%f 帧类型:%s", diff, type);
    }
    return nullptr;
}

void freePacket(AVPacket **pPacket) {
    av_packet_free(&*pPacket);
    av_free(*pPacket);
    *pPacket = nullptr;
}

void freeFrame(AVFrame **pAvFrame) {
    av_frame_free(&*pAvFrame);
    av_free(*pAvFrame);
    *pAvFrame = nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_nativelib_FFMpegPlay_release(JNIEnv *env, jobject thiz) {
    startDecode = false;
}