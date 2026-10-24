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
| FFmpeg | 统一解码 FLAC/WAV/APE/DSF/DFF | 待选 | ⬜ 未接入 | 需 configure 裁剪(只留音频 demuxer/decoder,去掉 video)以控体积 |
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

⬜ 骨架阶段,尚未接入任何库。JNI 链路验证通过后,优先接入 **FFmpeg**(系统通路播放的先决条件)。
