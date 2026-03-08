cmake_minimum_required(VERSION 3.22.1)

#导入头文件
include_directories(include)
include_directories(share)
include_directories(decoder)
include_directories(reader)
include_directories(utils)
include_directories(libyuv)
include_directories(manager)
include_directories(queue)
include_directories(config)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")


#当前目录下所有源文件配置到source字段上
aux_source_directory(. SOURCE)
aux_source_directory(share SHARESOURCE)
aux_source_directory(reader READSOURCE)
aux_source_directory(utils UTILSSOURCE)
aux_source_directory(libyuv YUVSOURCE)
aux_source_directory(manager MANAGERSOURCE)
aux_source_directory(decoder DECODESOURCE)
aux_source_directory(queue QUEUESOURCE)
aux_source_directory(include INCLUDESOURCE)
aux_source_directory(config CONFIGESOURCE)

project("play")

add_library(ffmpegplayer SHARED
        ${SOURCE}
        ${READSOURCE}
        ${SHARESOURCE}
        ${INCLUDESOURCE}
        ${UTILSSOURCE}
        ${MANAGERSOURCE}
        ${DECODESOURCE}
        ${QUEUESOURCE}
        ${CONFIGESOURCE}
        ${YUVSOURCE})

# 方法1：添加链接器选项 - 推荐用于CMake 3.13+
target_link_options(ffmpegplayer PRIVATE "-Wl,-z,max-page-size=16384")

# 同时添加common-page-size以确保完全对齐
target_link_options(ffmpegplayer PRIVATE "-Wl,-z,common-page-size=16384")

target_link_libraries(
        ffmpegplayer
        log
        ffmpeg
        android
        OpenSLES
        yuv)