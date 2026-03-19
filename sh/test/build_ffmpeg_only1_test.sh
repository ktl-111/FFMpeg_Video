#!/bin/sh

# 配置NDK路径，根据本机环境自己指定
ANDROID_NDK_ROOT=/home/lb/download/android-ndk-r21d
# ABI版本，自己指定
ABI_VERSION=24

# 设置编译文件后输出文件夹
OUTPUT=$(pwd)/android
# 清空输出文件夹
rm -rf "$OUTPUT"
# 新建输出文件夹
mkdir -p "$OUTPUT"

# 交叉工具链的路径
TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64

# ffmpeg配置
COMMON_SET="
  --enable-cross-compile \
  --target-os=android \
  --enable-small \
  --sysroot=$TOOLCHAIN/sysroot \
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
--disable-programs \
--disable-network \
--enable-hwaccels \
--disable-filters \
--enable-filter=fps \
--enable-filter=zscale \
--enable-filter=tonemap \
--enable-filter=format \
--enable-filter=eq \
--enable-libzimg \
--enable-pthreads \
  --disable-shared \
  --enable-static \
  --enable-neon \
  --enable-decoder=h264 \
  --enable-decoder=hevc \
  --enable-jni \
  --enable-small \
  --enable-mediacodec \
  --enable-decoder=h264_mediacodec \
  --enable-decoder=hevc_mediacodec \
  --enable-decoder=gif \
  --disable-nonfree \
  --enable-gpl "
# 配置configure并编译
X264ROOT="/home/lb/download/ffmpeg_kit/ffmpeg-kit/prebuilt/android-arm64/zimg"
build64(){
export PKG_CONFIG_PATH="/home/lb/download/ffmpeg_kit/ffmpeg-kit/prebuilt/android-arm64/pkgconfig"
X264_INCLUDE=$X264ROOT/include
X264_LIB=$X264ROOT/lib
    ARCH=aarch64
    ARCH_FLAGS="\
    --arch=$ARCH \
    --cross-prefix=$TOOLCHAIN/bin/aarch64-linux-android- \
    --cc=$TOOLCHAIN/bin/aarch64-linux-android$ABI_VERSION-clang \
    --prefix=$OUTPUT/aarch64 \
    "
    ARCH_OUTPUT="$OUTPUT/$ARCH/lib"
    rm -rf "$ARCH_OUTPUT"
    mkdir -p "$ARCH_OUTPUT"
    ./configure \
    ${COMMON_SET} \
    ${ARCH_FLAGS} \
    --extra-ldflags="-lm -lstdc++" \
    --pkg-config="pkg-config --static"

    make clean all
    make -j16
    make install
}

# 打包为单文件
package64(){
    ARCH=aarch64
    SHARE="/home/lb/download/android-ndk-r21d/sources/cxx-stl/llvm-libc++/libs/arm64-v8a"
    ARCH_OUTPUT="$OUTPUT/$ARCH/lib"
    GCC_L=$ANDROID_NDK_ROOT/toolchains/$ARCH-linux-android-4.9/prebuilt/linux-x86_64/lib/gcc/$ARCH-linux-android/4.9.x
    SYSROOT_L=$TOOLCHAIN/sysroot/usr/lib/$ARCH-linux-android
    echo "SYSROOT_L $SYSROOT_L/$ABI_VERSION"
    $TOOLCHAIN/bin/$ARCH-linux-android-ld -L$ARCH_OUTPUT -L$GCC_L \
        -rpath-link=$SYSROOT_L/$ABI_VERSION -L$SYSROOT_L/$ABI_VERSION -L$SHARE -L$X264_LIB -soname libffmpeg.so \
        -shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o $ARCH_OUTPUT/libffmpeg.so \
        -lzimg \
        -lavcodec -lavfilter -lswresample -lavformat -lavutil -lswscale -lgcc \
    -lc -ldl -lm -lz -llog -landroid -lstdc++ \
    --dynamic-linker=/system/bin/linker
    
   


}

build64
package64

