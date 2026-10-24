#!/bin/bash
# FFmpeg Android arm64-v8a 交叉编译脚本
#
# 目的:为「幻辰 HiFi」编译一个**极致裁剪**的 FFmpeg —— 只留本地音频播放必需的
# demuxer/decoder,砍掉 video/encoder/network,控制 APK 体积与编译时间。
#
# 用法:
#   ./build_android.sh            # 完整编译(configure + make + install)
#   ./build_android.sh clean      # 清理产物重来
#
# 环境前提(见 third_party/README.md):
#   NDK 28.2.13676358(经 Rosetta 运行的 darwin-x86_64 工具链)
#
# 输出:
#   third_party/ffmpeg/prebuilt/arm64-v8a/{include,lib}

set -e  # 任何一步失败立即停,不留半成品

# ============================================================
# 路径与工具链配置
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/ffmpeg-7.1"                 # FFmpeg 源码
NDK="$HOME/Library/Android/sdk/ndk/28.2.13676358"
HOST_TAG="darwin-x86_64"                          # NDK 预置工具链(Intel,经 Rosetta 跑)
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"

# 目标:arm64-v8a,API 26(见 CLAUDE.md 第八节 minSdk)
TARGET_HOST="aarch64-linux-android"
API=26
CC="$TOOLCHAIN/bin/${TARGET_HOST}${API}-clang"
CXX="$TOOLCHAIN/bin/${TARGET_HOST}${API}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"
RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
STRIP="$TOOLCHAIN/bin/llvm-strip"
NM="$TOOLCHAIN/bin/llvm-nm"

# 输出目录(不入库,见 .gitignore)
OUTPUT_DIR="$SCRIPT_DIR/prebuilt/arm64-v8a"
BUILD_DIR="$SCRIPT_DIR/build/arm64-v8a"           # 独立 build 目录,不污染源码

# ============================================================
# clean 子命令
# ============================================================
if [ "$1" = "clean" ]; then
    echo "==> 清理构建与输出目录"
    rm -rf "$BUILD_DIR" "$OUTPUT_DIR"
    echo "==> 清理完成"
    exit 0
fi

# ============================================================
# 前置检查
# ============================================================
if [ ! -x "$CC" ]; then
    echo "❌ 找不到编译器: $CC" >&2
    echo "   请确认 NDK 路径与工具链是否存在(见 third_party/README.md)" >&2
    exit 1
fi
if [ ! -d "$SRC_DIR" ]; then
    echo "❌ 找不到 FFmpeg 源码: $SRC_DIR" >&2
    exit 1
fi

echo "============================================================"
echo "FFmpeg Android 交叉编译"
echo "  源码:     $SRC_DIR"
echo "  编译器:   $CC"
echo "  目标:     $TARGET_HOST (API $API)"
echo "  输出:     $OUTPUT_DIR"
echo "============================================================"

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"
cd "$BUILD_DIR"

# ============================================================
# configure:极致裁剪
# ------------------------------------------------------------
# 思路:
#   1) --disable-everything 关掉所有组件,再按需 --enable 单个
#      → 保证产物最小,不含任何用不到的东西
#   2) demuxer:本地音频容器;decoder:对应音频解码;protocol:仅 file
#   3) 禁用 encoder/muxer(只播放不转码)、device、network、doc、debug
#   4) 生成共享库(.so),禁用静态库与程序(ffmpeg/ffprobe/ffplay 命令行不需要)
# ============================================================
"$SRC_DIR/configure" \
    --prefix="$OUTPUT_DIR" \
    \
    --target-os=android \
    --arch=aarch64 \
    --cpu=armv8-a \
    --cross-prefix="$TOOLCHAIN/bin/llvm-" \
    --cc="$CC" \
    --cxx="$CXX" \
    --ar="$AR" \
    --ranlib="$RANLIB" \
    --strip="$STRIP" \
    --nm="$NM" \
    --sysroot="$TOOLCHAIN/sysroot" \
    --extra-ldflags="-L$TOOLCHAIN/sysroot/usr/lib/$TARGET_HOST/$API" \
    \
    --enable-shared \
    --disable-static \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --disable-network \
    --disable-devices \
    --disable-postproc \
    --disable-avdevice \
    --disable-avfilter \
    --disable-encoders \
    --disable-muxers \
    --disable-hwaccels \
    --disable-asm \
    --disable-inline-asm \
    \
    --enable-small \
    --enable-optimizations \
    \
    --disable-everything \
    \
    `# —— protocol:本地文件播放只需 file ——` \
    --enable-protocol=file \
    \
    `# —— demuxer:本地音频容器(已逐一在 FFmpeg 7.1 源码验证存在) ——` \
    --enable-demuxer=flac \
    --enable-demuxer=ape \
    --enable-demuxer=dsf \
    --enable-demuxer=wav \
    --enable-demuxer=mp3 \
    --enable-demuxer=aac \
    --enable-demuxer=mov \
    --enable-demuxer=ogg \
    --enable-demuxer=caf \
    \
    `# —— decoder:PCM 系(WAV/AIFF/CAF 内嵌)、无损、DSD ——` \
    --enable-decoder=flac \
    --enable-decoder=ape \
    --enable-decoder=mp3 \
    --enable-decoder=aac \
    --enable-decoder=vorbis \
    --enable-decoder=opus \
    --enable-decoder=alac \
    \
    `# PCM 各类变体(WAV/AIFF 容器内可能是这些)` \
    --enable-decoder=pcm_s16le \
    --enable-decoder=pcm_s16be \
    --enable-decoder=pcm_s24le \
    --enable-decoder=pcm_s24be \
    --enable-decoder=pcm_s32le \
    --enable-decoder=pcm_s32be \
    --enable-decoder=pcm_f32le \
    --enable-decoder=pcm_f64le \
    --enable-decoder=pcm_u8 \
    --enable-decoder=pcm_s16le_planar \
    --enable-decoder=pcm_s24le_planar \
    --enable-decoder=pcm_s32le_planar \
    \
    `# DSD 解码(dsf/dff 核心)` \
    --enable-decoder=dsd_lsbf \
    --enable-decoder=dsd_msbf \
    --enable-decoder=dsd_lsbf_planar \
    --enable-decoder=dsd_msbf_planar \
    --enable-decoder=dst \
    \
    `# —— parser:解复用需要的解析器 ——` \
    --enable-parser=flac \
    --enable-parser=mpegaudio \
    --enable-parser=aac \
    --enable-parser=vorbis \
    --enable-parser=opus \
    \
    `# —— swresample:采样率/格式转换(FFmpeg 内部输出统一 PCM 用) ——` \
    --enable-swresample \
    \
    2>&1 | tail -25

echo "============================================================"
echo "✅ configure 完成,开始编译(make -j$(sysctl -n hw.ncpu))"
echo "   (经 Rosetta 转译,大库编译需要几分钟,请耐心)"
echo "============================================================"

# 编译并安装到 OUTPUT_DIR
make -j"$(sysctl -n hw.ncpu)" 2>&1 | tail -5
make install 2>&1 | tail -5

echo "============================================================"
echo "✅ 编译完成,产物:"
echo "============================================================"
ls -lh "$OUTPUT_DIR/lib"/*.so 2>/dev/null || echo "(无 .so 产出,检查 configure 是否成功)"
echo ""
echo "头文件目录: $OUTPUT_DIR/include"
du -sh "$OUTPUT_DIR" 2>/dev/null | awk '{print "产物总大小:", $1}'
