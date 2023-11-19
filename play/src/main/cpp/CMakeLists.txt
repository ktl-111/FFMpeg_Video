cmake_minimum_required(VERSION 3.22.1)

#导入头文件
include_directories(include)
include_directories(decoder)
include_directories(reader)
include_directories(utils)
include_directories(libyuv)
include_directories(manager)
include_directories(queue)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")


#当前目录下所有源文件配置到source字段上
aux_source_directory(. SOURCE)
aux_source_directory(reader READSOURCE)
aux_source_directory(utils UTILSSOURCE)
aux_source_directory(libyuv YUVSOURCE)
aux_source_directory(manager MANAGERSOURCE)
aux_source_directory(decoder DECODESOURCE)
aux_source_directory(queue QUEUESOURCE)


project("play")

add_library(ffmpegplayer SHARED
        ${SOURCE}
        ${READSOURCE}
        ${UTILSSOURCE}
        ${MANAGERSOURCE}
        ${DECODESOURCE}
        ${QUEUESOURCE}
        ${YUVSOURCE})

target_link_libraries(
        ffmpegplayer
        log
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
        android
        OpenSLES
        yuv)