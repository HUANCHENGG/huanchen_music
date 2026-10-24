# NDK 引擎层 CMake 构建脚本
# 当前为最小可编译骨架:先验证 JNI 链路通,FFmpeg/libusb 等第三方库后续挂入(见 third_party/README.md)
cmake_minimum_required(VERSION 3.22.1)
project(huanchen_engine LANGUAGES CXX)

# C++ 标准:音频引擎用 C++17(结构化绑定/optional 等提升可读性)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引擎层共享库
# 后续源文件会按 decoder/dsp/output/usb 子目录扩充(见 CLAUDE.md 第三节)
add_library(huanchen_engine SHARED
    jni_bridge.cpp
)

# 链接安卓系统库
# log:日志;android: native_app_glue 等;后续 USB 独占还需链接 OpenSL/AAudio
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(huanchen_engine
    ${log-lib}
    ${android-lib}
)
