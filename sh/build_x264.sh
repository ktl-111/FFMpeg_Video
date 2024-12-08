#!/bin/bash
 
export NDK=/home/lb/download/android-ndk-r21d #NDK path
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export API=21 #对应android sdk版本 
rm -rf android 
function build_android
{
APP_ABI=$1
  echo "======== > Start build $APP_ABI"
  case ${APP_ABI} in
  armeabi-v7a)
    HOST=armv7a-linux-android
    CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi-
    ;;
  arm64-v8a)
    HOST=aarch64-linux-android
    CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android-
    ;;
  esac
 
./configure \
	--prefix=$PREFIX \
	--disable-cli \
	--enable-shared \
	--enable-pic \
	--enable-static \
	--enable-strip \
	--cross-prefix=$CROSS_PREFIX \
	--sysroot=$SYSROOT \
	--host=$HOST
 
make clean
 
make -j4
 
make install
}
 
PREFIX=`pwd`/android/armeabi-v7a
SYSROOT=$TOOLCHAIN/sysroot
export TARGET=armv7a-linux-androideabi
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
 
build_android armeabi-v7a
 
PREFIX=`pwd`/android/arm64-v8a
export TARGET=aarch64-linux-android
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
 
build_android arm64-v8a
