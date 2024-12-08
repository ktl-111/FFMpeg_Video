#!/bin/bash
#配置你的NDK路径
#引用x264
export NDK=/home/lb/download/android-ndk-r21d
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
rm -rf androidOut
function build_android
{
./configure \
--prefix=$PREFIX \
--disable-encoders \
--disable-decoders \
--disable-postproc \
--disable-debug \
--disable-doc \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-symver \
--disable-doc \
--disable-avdevice \
--disable-static \
--disable-avdevice \
--disable-programs \
--disable-network \
--disable-hwaccels \
--disable-filters \
--enable-filter=fps \
--enable-pthreads \
--enable-gpl \
--enable-shared \
--enable-neon \
--enable-decoder=h264 \
--enable-decoder=hevc \
--enable-libx264 \
--enable-encoder=libx264 \
--enable-jni \
--enable-small \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--enable-decoder=hevc_mediacodec \
--enable-decoder=gif \
--cross-prefix=$CROSS_PREFIX \
--target-os=android \
--arch=$ARCH \
--cpu=$CPU \
--cc=$CC \
--cxx=$CXX \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $OPTIMIZE_CFLAGS -I$X264_INCLUDE " \
--extra-ldflags="-L$X264_LIB $ADDI_LDFLAGS" \
--pkg-config="pkg-config --static" \

make clean
make -j16
make install
}

#arm64-v8a 参数配置

# 指定X264的库
export PKG_CONFIG_PATH=/home/lb/download/libx264/x264/android/arm64-v8a/lib/pkgconfig
X264_INCLUDE=/home/lb/download/libx264/x264/android/arm64-v8a/include
X264_LIB=/home/lb/download/libx264/x264/android/arm64-v8a/lib
ARCH=arm64
CPU=armv8-a
API=21
CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
SYSROOT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android-
PREFIX=$(pwd)/androidOut/$CPU
OPTIMIZE_CFLAGS="-march=$CPU"

echo "PKG_CONFIG_PATH:${PKG_CONFIG_PATH}"
#清空上次的编译
make clean
# 函数调用
build_android

echo "============================build android ffmpeg arm64 end=========================="

#arm-v7a 参数配置

# 指定X264的库
export PKG_CONFIG_PATH=/home/lb/download/libx264/x264/android/armeabi-v7a/lib/pkgconfig
echo "PKG_CONFIG_PATH:${PKG_CONFIG_PATH}"

X264_INCLUDE=/home/lb/download/libx264/x264/android/armeabi-v7a/include
X264_LIB=/home/lb/download/libx264/x264/android/armeabi-v7a/lib

ARCH=arm
CPU=armv7a
API=21
CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
SYSROOT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi-
PREFIX=$(pwd)/androidOut/$CPU
OPTIMIZE_CFLAGS="-march=$CPU"

#清空上次的编译
make clean
# 函数调用
build_android

echo "============================build android ffmpeg armv7a end=========================="
