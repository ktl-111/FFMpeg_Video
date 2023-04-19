#屏蔽整个 CRYPTO 宏定义分支 : 即不定义 CRYPTO 宏 , 不进入整个分支,否则hashswf.c会进入include相关宏块
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_CRYPTO"  )
#遍历所有.c文件,定义rtmp_source全局变量
file(GLOB rtmp_source *.c)
#生成静态库rtmp
add_library(rtmp STATIC ${rtmp_source})