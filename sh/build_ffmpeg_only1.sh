#!/bin/sh
# NDK 所在的路径
#引入x264/openh264 编译,合成一个so
export ANDROID_NDK_ROOT=/home/lb/download/android-ndk-r21d
TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64
OUT_PUT=$(pwd)/android
rm -rf $OUT_PUT

#OPENH264
ROOT264=/home/lb/download/libopen264/libs/openh264-out
CONFIG_ENABLE=libopenh264
A_FILENAME=libopenh264.a
#X264
#ROOT264=/home/lb/download/libx264/x264/android
#CONFIG_ENABLE=libx264
#A_FILENAME=libx264.a

build() {
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
--enable-neon \
--enable-decoder=h264 \
--enable-decoder=hevc \
--enable-$CONFIG_ENABLE \
--enable-encoder=$CONFIG_ENABLE \
--enable-jni \
--enable-small \
--enable-hwaccels \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--enable-decoder=hevc_mediacodec \
--enable-decoder=gif \
--cross-prefix=$CROSS_PREFIX- \
--target-os=android \
--arch=$ARCH \
--cpu=$CPU \
--cc=$CC \
--cxx=$CXX \
--enable-cross-compile \
--sysroot=$SYSROOT \
--disable-shared \
--enable-static \
--extra-cflags="-Os -fpic" \
--pkg-config="pkg-config --static" \

make clean
make -j16
make install
}
package_library() {
$CROSS_PREFIX-ld -L$PREFIX/lib -L$GCC_L \
-rpath-link=$SYSROOT_L/$API -L$SYSROOT_L/$API -soname libffmpeg.so \
-shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o $PREFIX/libffmpeg.so \
$LINK_264 \
-lavcodec -lavfilter -lswresample -lavformat -lavutil -lswscale -lgcc -lc -ldl -lm -lz -llog -landroid -lstdc++ \
--dynamic-linker=/system/bin/linker
}

#arm64-v8a 参数配置

# 指定264的库
export PKG_CONFIG_PATH=$ROOT264/arm64-v8a/lib/pkgconfig
LIB264_INCLUDE=$ROOT264/arm64-v8a/include
LIB264_LIB=$ROOT264/arm64-v8a/lib
LINK_264=$LIB264_LIB/$A_FILENAME

ARCH=aarch64
CPU=armv8a
API=21
CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
GCC_L=$ANDROID_NDK_ROOT/toolchains/$ARCH-linux-android-4.9/prebuilt/linux-x86_64/lib/gcc/aarch64-linux-android/4.9.x
SYSROOT=$TOOLCHAIN/sysroot
SYSROOT_L=$SYSROOT/usr/lib/aarch64-linux-android
CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android
PREFIX=$OUT_PUT/$CPU

build
package_library
echo "build arm64-v8a done"

#arm-v7a 参数配置

# 指定264的库
export PKG_CONFIG_PATH=$ROOT264/armeabi-v7a/lib/pkgconfig
LIB264_INCLUDE=$ROOT264/armeabi-v7a/include
LIB264_LIB=$ROOT264/armeabi-v7a/lib
LINK_264=$LIB264_LIB/$A_FILENAME

ARCH=arm
CPU=armv7a
API=21
CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
GCC_L=$ANDROID_NDK_ROOT/toolchains/$ARCH-linux-androideabi-4.9/prebuilt/linux-x86_64/lib/gcc/arm-linux-androideabi/4.9.x
SYSROOT=$TOOLCHAIN/sysroot
SYSROOT_L=$SYSROOT/usr/lib/arm-linux-androideabi
CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi
PREFIX=$OUT_PUT/$CPU

build
package_library
echo "build arm-v7a done"

