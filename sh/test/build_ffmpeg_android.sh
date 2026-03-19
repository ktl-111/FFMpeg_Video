#!/bin/sh
# NDK 所在的路径
export NDK=/home/lb/download/android-ndk-r21d
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
OUT_PUT=$(pwd)/android
rm -rf $OUT_PUT
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
--enable-jni \
--enable-small \
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
echo "package_library"
$CROSS_PREFIX-ld \
-rpath-link=$SYSROOT_L/$API -L$SYSROOT_L/$API -L$PREFIX/lib -L$GCC_L \
-soname libffmpeg.so \
-shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o $PREFIX/libffmpeg.so \
-lavcodec -lavfilter -lswresample -lavformat -lavutil -lswscale -lgcc \
-lc -ldl -lm -lz -llog \
--dynamic-linker=/system/bin/linker

}

X264ROOT="/home/lb/download/libx264/x264/android"

#arm64-v8a 参数配置

# 指定X264的库
#export PKG_CONFIG_PATH=$X264ROOT/arm64-v8a/lib/pkgconfig
#X264_INCLUDE=$X264ROOT/arm64-v8a/include
#X264_LIB=$X264ROOT/arm64-v8a/lib
#ARCH=aarch64
#CPU=armv8a
#API=21
#CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
#CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
#GCC_L=$TOOLCHAIN/lib/gcc/aarch64-linux-android/4.9.x
#SYSROOT=$TOOLCHAIN/sysroot
#SYSROOT_L=$SYSROOT/usr/lib/aarch64-linux-android
#CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android
#PREFIX=$OUT_PUT/$CPU
#OPTIMIZE_CFLAGS="-march=$CPU"

#make clean
#build
#make clean
#package_library
#echo "build arm64-v8a done"

#arm-v7a 参数配置

# 指定X264的库
export PKG_CONFIG_PATH=$X264ROOT/armeabi-v7a/lib/pkgconfig
X264_INCLUDE=$X264ROOT/armeabi-v7a/include
X264_LIB=$X264ROOT/armeabi-v7a/lib
ARCH=arm
CPU=armv7a
API=26
CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
GCC_L=$TOOLCHAIN/lib/gcc/arm-linux-androideabi/4.9.x/armv7-a/thumb
SYSROOT=$TOOLCHAIN/sysroot
SYSROOT_L=$SYSROOT/usr/lib/arm-linux-androideabi
CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi
PREFIX=$OUT_PUT/$CPU
OPTIMIZE_CFLAGS="-march=$CPU"

make clean
build
make clean
package_library
echo "build arm-v7a done"

