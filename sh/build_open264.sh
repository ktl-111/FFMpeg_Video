#!/bin/bash

#git clone https://github.com/cisco/openh264
# 需要指定 ANDROID_NDK 和 ANDROID_NDK
export ANDROID_SDK=/home/lb/download/android_sdk
export ANDROID_NDK=/home/lb/download/android-ndk-r21d
export PATH=$ANDROID_SDK/tools:$PATH

function build_openh264 {
  ABI=$1
  API_LEVEL=$2

  case $ABI in
    armeabi-v7a )
      ARCH=arm
      ;;
    arm64-v8a )
      ARCH=arm64
      ;;
    x86 )
      ARCH=x86
      ;;
    x86_64 )
      ARCH=x86_64
      ;;
  esac

  TARGET_OS=android
  ANDROID_TARGET=android-$API_LEVEL
  BUILD_PREFIX=$ROOT_PATH/$ABI

  echo "build libopenh264 ${ABI} ${ANDROID_TARGET}"
  echo "build libopenh264 ${ABI} output : ${BUILD_PREFIX}"

  make \
      OS=${TARGET_OS} \
      NDKROOT=$ANDROID_NDK \
      TARGET=$ANDROID_TARGET \
      ARCH=$ARCH \
      clean
  make \
      OS=${TARGET_OS} \
      NDKROOT=$ANDROID_NDK \
      TARGET=$ANDROID_TARGET \
      NDKLEVEL=$API_LEVEL \
      ARCH=$ARCH \
      PREFIX=$BUILD_PREFIX \
      -j4 install
}


ROOT_PATH=$(pwd)/openh264-out
rm -rf $ROOT_PATH
mkdir $ROOT_PATH
make clean
build_openh264 arm64-v8a 21

make clean
build_openh264 armeabi-v7a 21
