cmake_minimum_required(VERSION 3.22.1)
#导入头文件
include_directories(include)
include_directories(share)
include_directories(reader)
include_directories(utils)
include_directories(libyuv)
message("路径:${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")
#设置动态依赖库,相当于将路径配置到环境变量,后续target_link_libraries可直接使用
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")

#当前目录下所有源文件配置到source字段上
aux_source_directory(. SOURCE)
aux_source_directory(reader READSOURCE)
aux_source_directory(utils UTILSSOURCE)

project("nativelib")
#将nativelib.cpp代码动态编译成libnativelib.so文件
add_library(
        nativelib
        SHARED
        ${SOURCE}
        ${READSOURCE}
        ${UTILSSOURCE}
        libyuv/libyuv.h
)
#引入librtmp下的CMakeLists
add_subdirectory(librtmp)

target_link_libraries(
        nativelib
        log
        rtmp
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
        android
        OpenSLES
        yuv
)