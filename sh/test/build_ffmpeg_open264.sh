#!/bin/bash
#配置你的NDK路径
export NDK=/home/lb/download/android-ndk-r21d
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
rm -rf androidOut
function build_android
{
./configure \
--prefix=$PREFIX \
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
--enable-shared \
--enable-neon \
--enable-libopenh264 \
--enable-encoder=libopenh264 \
--enable-hwaccels \
--enable-jni \
--enable-small \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--enable-decoder=hevc_mediacodec \
--enable-decoder=mpeg4_mediacodec \
--enable-hwaccel=h264_mediacodec \
--cross-prefix=$CROSS_PREFIX \
--target-os=android \
--arch=$ARCH \
--cpu=$CPU \
--cc=$CC \
--cxx=$CXX \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $OPTIMIZE_CFLAGS " \
--extra-ldflags="-L$X264_LIB $ADDI_LDFLAGS" \
--pkg-config="pkg-config --static" \

make clean
make -j16
make install
}

#arm64-v8a 参数配置

# 指定X264的库
export PKG_CONFIG_PATH=/home/lb/download/libopen264/openh264
X264_INCLUDE=/home/lb/download/libopen264/openh264
X264_LIB=/home/lb/download/libopen264/openh264
ARCH=arm64
CPU=armv8-a
API=21
CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
SYSROOT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android-
PREFIX=$(pwd)/androidOut/$CPU
OPTIMIZE_CFLAGS="-march=$CPU"

#清空上次的编译
make clean
# 函数调用
build_android

echo "============================build android ffmpeg arm64 end=========================="

