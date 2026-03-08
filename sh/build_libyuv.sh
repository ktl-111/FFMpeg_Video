#!/bin/bash

ANDROID_NDK=/home/lb/download/android-ndk-r24
LIBYUV_SRC=$(pwd)

# 需要编译的 ABI 列表
ABIS=("armeabi-v7a" "arm64-v8a")

# Android API Level
API_LEVEL=24

# 只启用旋转、裁剪、缩放功能的宏定义
ENABLE_FEATURES="-DLIBYUV_DISABLE_JPEG -DLIBYUV_DISABLE_NEON_ROW -DCOMPILE_SSL=OFF"

for ABI in "${ABIS[@]}"; do
    echo "========================================="
    echo "Building for ABI: ${ABI}"
    echo "========================================="

    BUILD_DIR="build_${ABI}"
    OUTPUT_DIR="output/${ABI}"

    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${OUTPUT_DIR}"

    cd "${BUILD_DIR}"

    # 根据不同的ABI设置编译选项
    if [ "${ABI}" = "arm64-v8a" ]; then
        # arm64-v8a: 启用dotprod和i8mm扩展以支持usdot指令
        C_FLAGS="-fPIC -O2 -march=armv8.2-a+dotprod+i8mm ${ENABLE_FEATURES}"
    else
        # armeabi-v7a: 使用NEON优化
        C_FLAGS="-fPIC -O2 -march=armv7-a -mfpu=neon ${ENABLE_FEATURES}"
    fi

    cmake "${LIBYUV_SRC}" \
        -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="${ABI}" \
        -DANDROID_PLATFORM="android-${API_LEVEL}" \
        -DANDROID_NDK="${ANDROID_NDK}" \
        -DANDROID_STL=c++_static \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384 -Wl,--gc-sections" \
        -DCMAKE_C_FLAGS="${C_FLAGS}" \
        -DCMAKE_CXX_FLAGS="${C_FLAGS}" \
        -DBUILD_WITH_JPEG=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    # 编译
    ninja -j$(nproc)

    # 使用strip减小体积
    "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" libyuv.so

    # 拷贝产物
    cp libyuv.so "${LIBYUV_SRC}/${OUTPUT_DIR}/"

    # 显示文件大小
    echo "✅ ${ABI} build completed: $(du -h ${LIBYUV_SRC}/${OUTPUT_DIR}/libyuv.so | cut -f1)"

    cd "${LIBYUV_SRC}"
done

echo ""
echo "========================================="
echo "All builds completed. Output in: output/"
echo "File sizes:"
for ABI in "${ABIS[@]}"; do
    if [ -f "output/${ABI}/libyuv.so" ]; then
        echo "  - ${ABI}: $(du -h output/${ABI}/libyuv.so | cut -f1)"
    fi
done
echo "========================================="