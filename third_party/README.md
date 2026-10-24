# third_party —— NDK 预编译依赖库

> 这里存放音频引擎所需的 C/C++ 第三方库的**源码/预编译产物**及其编译说明。
> 编译产物(.so/.a)**不入库**(见根 .gitignore),只入库编译脚本与本说明。

## 本机工具链(已确认)

- **宿主机**: macOS, Apple Silicon (arm64)
- **NDK**: 28.2.13676358 → `~/Library/Android/sdk/ndk/28.2.13676358`
- **NDK 预置工具链**: `darwin-x86_64`(Intel 版,经 Rosetta 2 转译运行,**可用但编译较慢**;后续嫌慢可换原生 ARM 工具链)
- **CMake**: 3.22.1 → `~/Library/Android/sdk/cmake/3.22.1`
- **目标 ABI**: 一期只打 `arm64-v8a`(见 CLAUDE.md 第八节)

## 依赖清单(待接入)

| 库 | 用途 | 版本 | 状态 | 编译说明 |
|---|---|---|---|---|
| FFmpeg | 统一解码 FLAC/WAV/APE/DSF/DFF | **7.1** | ✅ **已接入(arm64-v8a)** | `build_android.sh` 极致裁剪(仅音频 demuxer/decoder/file protocol),产物 3.5M |
| libusb | USB 独占(路线A)用户态 USB 栈 | 待选 | ⬜ 未接入 | Android 下经 `libusb_wrap_sys_device()` 包装 UsbDeviceConnection 的 fd |
| libsoxr | 高质量重采样(DSP 层) | 待选 | ⬜ 未接入 | CMake 工程,可直接 add_subdirectory |
| TagLib | 元数据/封面解析(dsf→ID3v2, ape→APEv2) | 待选 | ⬜ 未接入 | CMake 工程,只编 taglib 核心即可 |
| dsd2pcm | DSD→PCM 转换(质量优于 swresample) | 待选 | ⬜ 未接入 | 单文件库,可直接编进引擎 |

## 编译约定

- 每个库一个子目录(如 `third_party/ffmpeg/`),内含 `build_android.sh` 或 CMake 配置。
- 统一输出到 `third_party/<lib>/prebuilt/arm64-v8a/`(头文件 + .so/.a)。
- app 模块经 `app/src/main/cpp/CMakeLists.txt` 以 `add_library(... IMPORTED)` 引用。
- **编译前先想清楚裁剪项**:FFmpeg 默认全量极大,必须只保留需要的 demuxer/decoder/protocol,否则 APK 体积爆炸。

## 当前状态

✅ **FFmpeg 7.1 已接入**:`build_android.sh` 完成交叉编译,`prebuilt/arm64-v8a/` 产物 3.5M,经 CMake IMPORTED 链接进 `huanchen_engine`,APK 打包验证通过。

**FFmpeg 7.1 裁剪明细**(`--disable-everything` 后按需开启):
- demuxer: `flac ape dsf wav mp3 aac mov ogg caf`
- decoder: `flac ape mp3 aac vorbis opus alac` + `pcm_*` 系列 + `dsd_lsbf/msbf(_planar)` + `dst`
- parser: `flac mpegaudio aac vorbis opus`
- protocol: 仅 `file`(本地播放)
- 全砍: encoder / muxer / avfilter / avdevice / network / hwaccels / asm / programs
- License: **LGPL 2.1**(无 GPL 组件,自用/商用都干净)
- 产物打入 APK 路径: `prebuilt/jniLibs/arm64-v8a/*.so`(符号链接 → 实际产物)

⬜ 下一个待接入: **libusb**(USB 独占路线A)或 **TagLib**(媒体库元数据,见效快)。
