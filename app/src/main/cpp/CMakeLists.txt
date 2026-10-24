# NDK 引擎层 CMake 构建脚本
# 引入 FFmpeg 预编译库(third_party/ffmpeg/prebuilt/arm64-v8a),链接进引擎层共享库。
cmake_minimum_required(VERSION 3.22.1)
project(huanchen_engine LANGUAGES CXX)

# C++ 标准:音频引擎用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# FFmpeg 预编译库路径(相对本 CMakeLists.txt: src/main/cpp/)
# ============================================================
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party")
set(FFMPEG_DIR "${THIRD_PARTY_DIR}/ffmpeg/prebuilt/${ANDROID_ABI}")
set(FFMPEG_INCLUDE "${FFMPEG_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_DIR}/lib")

# 校验 FFmpeg 产物存在,缺失时给出明确报错而非静默链接失败
if(NOT EXISTS "${FFMPEG_LIB_DIR}/libavcodec.so")
    message(FATAL_ERROR
        "FFmpeg 预编译库缺失: ${FFMPEG_LIB_DIR}/libavcodec.so\n"
        "请先运行 third_party/ffmpeg/build_android.sh 完成交叉编译")
endif()

# 头文件目录(引擎层源码 #include <libavcodec/...>)
include_directories("${FFMPEG_INCLUDE}")

# 以 IMPORTED 方式引入各 FFmpeg 共享库
# 注意:链接顺序有依赖关系 —— avformat 依赖 avcodec,avcodec 依赖 avutil
# IMPORTED 库的符号解析交给链接器,但声明顺序保持:util 在前
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavutil.so")

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libswresample.so")

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavcodec.so")

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavformat.so")

# ============================================================
# 引擎层共享库
# 后续源文件会按 decoder/dsp/output/usb 子目录扩充(见 CLAUDE.md 第三节)
# ============================================================
add_library(huanchen_engine SHARED
    jni_bridge.cpp
)

# 链接安卓系统库
find_library(log-lib log)
find_library(android-lib android)

# 链接顺序:先本库依赖的 FFmpeg,再系统库
target_link_libraries(huanchen_engine
    avformat
    avcodec
    swresample
    avutil
    ${log-lib}
    ${android-lib}
)
