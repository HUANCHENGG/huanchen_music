# CLAUDE.md — 幻辰 HiFi 音乐播放器

> 本文件是项目的最高规范。任何代码、目录、命名、提交都必须遵守本文件。
> 需要调整规范时:**先改本文件,再改实践**,不要反过来。
> 最后更新:2026-10-23(项目初始化)

---

## 一、项目是什么

幻辰自用的安卓 HiFi 本地音乐播放器。体验对标 Apple Music 审美,核心是**完全掌控**。
一期目标:本地 FLAC/DSD/APE/WAV 播放 + USB 独占(真 bit-perfect)+ Apple Music 风 UI(封面驱动/模糊背景/滚动歌词)。

**技术栈**: Kotlin + Jetpack Compose + NDK(C++)。
**目标机**: Android 15 (API 35),minSdk 见下。

---

## 二、铁律(不可违背)

1. **约束先行**: 新目录先定结构约定再动手;规范变更先改本文件。
2. **结论先行、不谄媚**: 回复以【幻辰】开头;不夸想法,给真实判断;方案有问题直接指出。
3. **改完主动验证**: 见第九节"验证命令",只改不验等于没做。
4. **不绕过报错**: 不为让代码跑通而注释报错/加 `@Suppress`/忽略警告,找根本原因。
5. **密钥不进代码/不进 commit/不进日志**(本项目暂无密钥,新增时遵守)。
6. **红线操作先问**: 删文件/目录/git 历史、改 CI 配置、git push/rebase/reset --hard、装全局依赖、公开发布——即使 auto-accept 也先停下问。
7. **中文注释**: 给新工程师看,注释用中文,解释"为什么"而非"是什么";代码/命令/变量名用英文。

---

## 三、目录结构约定

```
huanchen_music/
├── CLAUDE.md                 # 本文件,最高规范
├── settings.gradle.kts       # 模块声明 + 仓库
├── build.gradle.kts          # 根构建脚本(版本目录引用)
├── gradle.properties         # 构建属性
├── local.properties          # 本地 SDK/NDK 路径(不入库)
├── gradle/
│   ├── libs.versions.toml    # 版本目录(所有依赖版本统一在这)
│   └── wrapper/              # gradle wrapper
├── docs/                     # 项目文档(需求/架构/调研)
│   └── plan.md               # 一期需求与实施计划(权威需求来源)
├── app/                      # 唯一应用模块(一期单模块,见模块边界)
│   ├── build.gradle.kts
│   ├── proguard-rules.pro
│   └── src/
│       ├── main/
│       │   ├── AndroidManifest.xml
│       │   ├── java/com/huanchen/music/
│       │   │   ├── MainActivity.kt
│       │   │   ├── HuanchenApp.kt        # Application
│       │   │   ├── ui/                   # Compose UI 层
│       │   │   │   ├── theme/            # 主题引擎(ColorScheme 三态/封面取色)
│       │   │   │   ├── player/           # 播放页(封面/模糊背景/滚动歌词)
│       │   │   │   ├── library/          # 资料库(专辑/艺术家/列表)
│       │   │   │   └── settings/         # 设置页(独占开关/主题选择)
│       │   │   ├── player/               # 控制层: PlayerService/MediaSession/状态机
│       │   │   ├── library/              # 媒体库: 扫描/元数据/Room
│       │   │   ├── audio/                # 音频控制: 格式协商/通路切换/USB 设备管理
│       │   │   └── bridge/               # JNI 桥接(Kotlin ↔ native)
│       │   ├── cpp/                      # NDK 引擎层(C++)
│       │   │   ├── CMakeLists.txt
│       │   │   ├── decoder/              # FFmpeg 解码(统一 IDecoder)
│       │   │   ├── dsp/                  # dsd2pcm / libsoxr / DoP 打包
│       │   │   ├── output/               # IAudioOutput: SystemOutput / UsbExclusiveOutput
│       │   │   ├── usb/                  # libusb + UAC 协议栈(路线A独占)
│       │   │   └── jni_bridge.cpp        # JNI 入口
│       │   └── res/                      # 资源
│       ├── test/                         # 单元测试(JVM)
│       └── androidTest/                  # 仪器测试(真机)
└── third_party/              # NDK 预编译依赖(FFmpeg/libusb/soxr/taglib/dsd2pcm)
    └── README.md             # 说明各库版本/编译方式/来源
```

**约定说明**:
- **一期单模块**: 不为"显得专业"过度拆分 module。用 `ui / player / library / audio / bridge` 包结构划边界,够一期用;真到编译慢/耦合痛再拆。
- **版本目录统一**: 所有依赖版本只写 `gradle/libs.versions.toml`,不散落各 build 脚本。
- **`local.properties` 与 `third_party/` 产物不入库**(见 .gitignore)。
- **`docs/plan.md` 是需求权威来源**: 与本文件冲突时,需求以 plan.md 为准、工程规范以本文件为准。

---

## 四、模块边界(依赖方向,不可反向)

```
ui(Compose)
  → player(控制层: Service/状态机)
    → audio(格式协商/通路切换/USB 管理)
    → library(扫描/元数据/Room)
      → bridge(JNI) → cpp(引擎层: decoder/dsp/output/usb)
```

- **UI 不直接碰 JNI / cpp**;必须经 `player`/`audio` 控制层。
- **cpp 引擎层不知道 UI 存在**;解码层不知道输出走 USB 还是系统(只塞 Ring Buffer)。
- **格式协商(OutputPlan)由控制层在打开输出前完成**,不在解码层做。
- 跨层通信: Kotlin 内部用 StateFlow/回调;Kotlin↔native 只走 `bridge/` 包,不散落 JNI 调用。

---

## 五、命名约定

| 类型 | 约定 | 示例 |
|---|---|---|
| 包名 | 全小写 | `com.huanchen.music.audio` |
| 类/接口 | UpperCamelCase | `OutputModeNegotiator`, `IAudioOutput` |
| 接口 | C++ 侧允许 `I` 前缀 | `IDecoder`, `IAudioOutput` |
| 函数/变量 | lowerCamelCase | `negotiateOutput()`, `dacCapability` |
| Composable | UpperCamelCase 名词 | `PlayerScreen()`, `LyricLineItem()` |
| 常量 | UPPER_SNAKE_CASE | `DEFAULT_BUFFER_MS` |
| C++ 文件 | snake_case | `usb_exclusive_output.cpp` |
| Kotlin 文件 | 与主类同名 | `OutputModeNegotiator.kt` |
| 资源 | snake_case 带前缀 | `ic_play`, `bg_player_scrim` |

---

## 六、核心技术决策(已定,勿随意推翻)

> 详细论证见 `docs/plan.md` 第四节。这里只列结论。

- **解码**: 统一 FFmpeg demux+decode 一条管线(FLAC/WAV/APE/DSF/DFF),输出统一 PCM。**不依赖 MediaCodec**(24bit 厂商不一致,bit-perfect 不可依赖)。
- **DSD**: FFmpeg 解到 DSD 格式 → `dsd2pcm` 转 PCM(质量优于 swresample);直出优先 DoP(兼容广),Native 仅路线A+支持的 DAC。优先级 Native→DoP→转PCM。
- **USB 独占**: 一期主攻**路线 A**(libusb + 自实现 UAC 等时传输,含 feedback 时钟);路线 B(Android 15 `setPreferredMixerAttributes`)作兜底。设置开关: 开=真 bit-perfect(锁采样率、绕过系统混音),关=普通通路。
- **多小尾巴自适应**: `DacCapability`(能力表)+ `OutputModeNegotiator`(协商),能力缓存存 Room(key=vid+pid)。**这套架构第一天就建好。**
- **封面取色**: Palette API 提 vibrant → material-color-utilities `SchemeTonalSpot` 生成 M3 ColorScheme。**不用 M3 dynamicColorScheme**(那是跟系统壁纸)。
- **模糊背景**: 先降采样到 ≤256px 再小半径模糊(性能红线,规避 API33+ clamp bug)。API31+ RenderEffect,以下 toolkit 离线模糊+缓存。不用 Haze。
- **滚动歌词**: 自解析 .lrc(处理一行多时间戳),LazyColumn + contentPadding 上下半屏 + animateScrollToItem 居中 + isScrollInProgress 防抢滚动。
- **主题**: ThemeSource 三态(预设/跟随封面),AnimatedColorScheme 逐角色色 animateColorAsState 渐变,不重建 MaterialTheme。
- **元数据**: TagLib 统一(dsf→ID3v2, ape→APEv2),Room 存扫描库。

---

## 七、依赖管理

- 所有第三方库版本**只**写 `gradle/libs.versions.toml`,build 脚本里用 `libs.xxx` 引用。
- NDK 预编译库放 `third_party/`,用 CMake `add_library(... IMPORTED)` 引入,版本与编译方式记 `third_party/README.md`。
- **新增依赖前先想:有没有 androidx/官方现成方案?** 能不加第三方就不加;必须加则在 commit message 说明理由。
- 核心依赖基线: Compose BOM / androidx.media3?(**否**,音频链路自研,不用 ExoPlayer) / Room / DataStore / Palette / material-color-utilities / Coil(图片加载) / kotlinx-coroutines。

---

## 八、构建配置基线

- **JDK**: 17(环境已装 openjdk@17)。
- **compileSdk / targetSdk**: 35(Android 15)。
- **minSdk**: 26(覆盖主流,USB host 支持好;若用路线A则需权衡,实测后定,先记 26)。
- **ABI**: 一期只打 `arm64-v8a`(现代机+小尾巴场景,x86 仅模拟器调试可选)。
- **Kotlin**: 与 Compose BOM 匹配的 stable 版,写进版本目录。
- **NDK**: r26+ 或更新稳定版(待装,见下);CMake 3.22+。

---

## 九、验证命令(改完必须跑)

```bash
# 1. 编译(最基本,改了必跑)
./gradlew assembleDebug

# 2. 静态检查
./gradlew lint

# 3. 单元测试(改了逻辑/解析必跑)
./gradlew testDebugUnitTest

# 4. NDK 原生编译(改了 cpp 必跑,assembleDebug 会连带触发)
./gradlew :app:externalNativeBuildDebug

# 5. 真机安装 + 跑(功能验证,需连 Android 15 设备)
./gradlew installDebug
adb shell am start -n com.huanchen.music/.MainActivity
```

**验证底线**:
- 改 Kotlin/UI → 至少跑 1 + 3。
- 改 cpp/音频链路 → 至少跑 1 + 4,并真机验证出声。
- 改 USB 独占 → 必须真机插小尾巴验证 bit-perfect + 无爆音(无法用单元测试替代)。
- 具体功能验收标准见 `docs/plan.md` 第六节。

---

## 十、当前状态与下一步

- [x] 需求与架构定稿(`docs/plan.md`)
- [x] CLAUDE.md 立规(本文件)
- [x] 安装 NDK + CMake(NDK 28.2.13676358 + CMake 3.22.1,经 sdkmanager 命令行安装)
- [x] Gradle 骨架 + 版本目录 + .gitignore(gradle 8.9 + AGP 8.7.3 + Kotlin 2.1.0)
- [x] 最小可运行 APP 构建链路验证通过(`./gradlew assembleDebug` 成功,含 arm64-v8a native 库)
- [ ] git init + 首次提交(待用户确认后执行)
- [ ] third_party NDK 库交叉编译(FFmpeg 优先 → libusb/soxr/taglib/dsd2pcm)
- [ ] 按 plan.md 分模块实施(能力探测架构先行 → 系统通路播放 → UI → USB 独占)

---

*本文件随项目演进更新,每次规范变更都要更新"最后更新"日期。*
