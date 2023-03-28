cmake_minimum_required(VERSION 3.22.1)

project("nativelib")
#将nativelib.cpp代码动态编译成libnativelib.so文件
add_library(
        rtmplib
        SHARED
        rtmplib.cpp)
#引入librtmp下的CMakeLists
add_subdirectory(librtmp)
#在ndk目录下查找liblog.so,并赋值给log-lib变量
find_library(
        log-lib
        log)
#将变量log-lib链接到nativelib下
target_link_libraries(
        rtmplib
        ${log-lib}
        rtmp)