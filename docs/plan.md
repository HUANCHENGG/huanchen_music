# 幻辰 HiFi 音乐播放器 — 一期需求与实施计划

> 状态:需求与技术架构均已确认,待建项目规范(CLAUDE.md)后进入实施
> 最后更新:2026-10-23

---

## 一、Context(为什么做)

幻辰(单片机工程师,C 背景)要做一个**自用**的安卓 HiFi 本地音乐播放器。

**核心动机(已确认)**:
1. 完全掌控体验 —— 现有 APP 不开放,想自己定义交互与视觉
2. 现有 HiFi APP 太丑/太土 —— 想要 Apple Music 级别的现代审美

**不是**要做一个功能更全的复制品去和海贝/UAPP 竞争,而是做一个**体验完全贴合自己**的私人播放器。

---

## 二、一期需求画像(苏格拉底式提问确认结果)

| 维度 | 决定 | 备注 |
|---|---|---|
| 核心动机 | 完全掌控体验 + 现有 APP 太丑 | 体验主导,非功能堆砌 |
| UI 方向 | 专辑封面驱动 + 高斯模糊背景 | Apple Music 式;明确要滚动歌词 |
| 一期掌控重点 | 视觉主题/动效(**收敛为预设主题+封面变色**) | 明确放弃"主题商店级深度定制" |
| EQ/DSP | **二期** | 一期专注播放+UI+独占 |
| 小尾巴 | 多条/型号未定 | **USB 模块必须做"探测设备能力→自适应输出模式"的通用架构** |
| USB 独占 | 设置可开关;**开了必须真 bit-perfect** | 需同时维护独占/普通两条音频通路,运行时切换 |
| 技术栈 | Kotlin + Jetpack Compose + NDK | 用户 C 背景,可参与 NDK 音频底层 |
| 媒体库 | 手动选定文件夹(升级为:**定目录 + 自动增量扫描**) | 兼顾可控与"日用不费劲" |
| 滚动歌词 | **一期就要**(lrc 逐行高亮滚动) | 用户明确要,接受开发量 |
| 一期砍掉 | 在线流媒体、云同步、车机/手表 | 纯本地 |
| 一期成功标准 | **能日用** | 媒体库+播放列表+播放页+稳定性全到位 |

---

## 三、一期范围(关进笼子)

### 包含(MUST)
- [ ] 本地媒体库:手动选定根目录 + 自动增量扫描
- [ ] 格式支持:FLAC / DSD(.dsf/.dff)/ APE / WAV(至少四种)
- [ ] 元数据与封面解析(标签、内嵌封面)
- [ ] 播放内核:解码(NDK)→ 输出
- [ ] USB 独占:**路线 A 真独占(libusb+UAC)**,设置开关,开启时 bit-perfect;多小尾巴自适应(能力表+协商);路线 B(Android15)作兜底
- [ ] UI:Apple Music 风(封面驱动 + 模糊背景 + 滚动歌词 + 预设主题)
- [ ] 播放列表 / 资料库(专辑、艺术家视图)
- [ ] 设置页(含 USB 独占开关、主题选择)

### 明确不做(二期及以后)
- 在线流媒体、云同步、车机/手表
- EQ / DSP 音频调校
- 主题商店级深度定制(一期只做预设主题+封面变色)
- Android Auto

---

## 四、技术架构(已调研确认)

### 4.1 整体音频链路(分层,解耦核心)

```
UI(Compose)
  └─ 控制层(Kotlin): PlayerService / MediaSession / USB 设备管理 / 格式协商
       └─ JNI
            └─ 引擎层(C++/NDK):
                 Decoder(FFmpeg,统一 IDecoder 接口)
                   → DSP/转换层(dsd2pcm / libsoxr 重采样 / DoP 打包,可插拔)
                   → Ring Buffer(解码端 2~4s)
                   → IAudioOutput 接口
                        ├─ SystemOutput(AAudio,普通通路/路线B兜底)
                        └─ UsbExclusiveOutput(libusb + UAC 栈,真独占/路线A)
```

**解耦核心:解码层不知道输出走 USB 还是系统,只往 Ring Buffer 塞统一格式 PCM;格式协商(OutputPlan)由控制层在打开输出前完成。切换独占 = 重建 output + 重新协商格式。**

**线程模型**: 解码线程(普通优先级)+ 输出线程(SCHED_FIFO 高优先级;USB 独占驱动等时发包节奏)。

### 4.2 USB 独占(路线 A,一期主攻;路线 B 兜底)

- **路线 A(一期主攻)**: `UsbManager.requestPermission()` 运行时授权(per-device)→ `UsbDeviceConnection.getFileDescriptor()` 拿 fd → native 层 `libusb_wrap_sys_device()` 包装 → 自实现 UAC1/UAC2 等时传输直推流,绕过 AudioFlinger,达成 bit-perfect。
  - 权限: `<uses-feature android.hardware.usb.host required=false>`;可选 device_filter.xml 实现插入自动唤起。
  - claim Audio 接口(bInterfaceClass=0x01)。**风险:系统可能已 claim,部分定制 ROM claim 失败 → 兼容性大头,需真机实测。**
  - **UAC2 asynchronous clock 必须实现 feedback endpoint,否则爆音(工程量最大点)。**
  - 权威参考: Linux 内核 `sound/usb/`(ALSA USB audio)的 clock/feedback 逻辑。
- **路线 B(Android 15 可用,作兜底)**: `AudioManager.setPreferredMixerAttributes()` + AudioTrack(DIRECT flag)。工程量小,插小尾巴开独占时优先探测,不支持降级路线 A / 普通通路。
- **独占开关语义(对齐你的需求)**: 设置里可开关。开=真 bit-perfect(锁采样率、绕过系统混音重采样);关=普通系统通路。

### 4.3 格式与解码库选型

| 格式 | 方案 | 备注 |
|---|---|---|
| FLAC / WAV / APE / DSF / DFF | **统一 FFmpeg demux+decode**,一条管线输出统一 PCM | 不依赖 MediaCodec(24bit 厂商不一致,bit-perfect 不可依赖) |
| DSD→PCM | `dsd2pcm` 专用库(质量优于 FFmpeg swresample) | 仅在不支持 DSD 直出的设备上用 |
| DSD 直出 | DoP(优先,兼容广)/ Native(仅路线A+支持的DAC) | DSD 优先级: Native→DoP→dsd2pcm转PCM |
| 元数据/封面 | TagLib(dsf 用 ID3v2,APE 用 APEv2) | Room 存扫描库 |
| 重采样 | libsoxr | DSP 层可插拔 |
| cue 分轨 | 自解析(libcue 或自写)+ 按 INDEX seek FFmpeg | 一期按需,可后置 |

**NDK 需交叉编译的库: FFmpeg + libusb + libsoxr + taglib(+dsd2pcm)。这是构建期的一大工作量,建议用 prefab/CMake 统一管理。**

### 4.5 能力探测 → 自动选择架构(多条小尾巴的关键)

- `DacCapability` 数据类: 采样率/位深/DSD模式/UAC版本/探测来源。
- 探测来源优先级: **路线 A 枚举 UAC 描述符(权威)** > 路线 B / AudioDeviceInfo(仅参考)。
- `OutputModeNegotiator`: 音轨格式 + DacCapability → OutputPlan。
- **设备能力缓存**: Room 存(key = vid+pid),支持用户手动覆盖 + 内置兼容数据库。协商失败兜底 44.1k/16bit。
- **这套架构从第一天就建好** —— 因为你有"多条小尾巴",自适应能力是整个 USB 模块的地基。

### 4.6 开源参考(无单一可抄,组合即蓝图)
- **Vanilla Music**: 播放器骨架/服务/状态机/扫描。
- **VLC for Android**: FFmpeg 深度集成范本。
- **ExoPlayer FFmpeg extension**: decoder/renderer 解耦思想。
- **libusb + Linux sound/usb/**: UAC 独占实现权威参考。
- UAPP/海贝闭源,仅功能对标。

### 4.7 一期整体风险排序
1. **USB 独占兼容性测试矩阵(最大工作量)** —— 不同小尾巴 + 不同 ROM 的 claim/反馈时钟表现。
2. 双通路切换时的状态一致性(爆音/断连)。
3. NDK 多库交叉编译(FFmpeg+libusb+soxr+taglib)。
4. 耗电(独占高优先级线程 + 大缓冲)。

### 4.4 UI 实现(Compose,已调研确认)

**核心模式:**
- **封面取色**: Palette API(`androidx.palette:palette-ktx`)提 vibrant 色 → 喂给 `material-color-utilities` 的 `SchemeTonalSpot` 生成整套 M3 ColorScheme。⚠️ 不用 M3 `dynamicColorScheme`(那是跟系统壁纸,不是封面)。按 songId 缓存 + 预取下一首。
- **模糊背景**: 静态封面场景 → API31+ 用 `RenderEffect.createBlurEffect`(经 `graphicsLayer`);API<31 用 `renderscript-intrinsics-replacement-toolkit` 离线模糊 Bitmap 并缓存。**性能红线:模糊输入 Bitmap 先降采样到 ≤256px 再小半径模糊**(规避 API33+ 大半径 clamp bug,且千元机也能 60fps)。切歌过渡用 `Crossfade`(600ms)。不用 Haze(封面是静态模糊,引入反而增加风险)。
- **滚动歌词**: 自解析 `.lrc`(正则,处理一行多时间戳)→ `LazyColumn` + `contentPadding(上下各半屏)` 让首末行也能居中 + `animateScrollToItem(带 offset 居中)`。当前行索引由 ViewModel 二分查找 + `distinctUntilChanged` 收敛(避免每帧重组)。`isScrollInProgress` 时暂停自动滚动、松手 3s 恢复 + "回到当前行"浮动按钮。行高亮:`animateFloatAsState` 缩放 1.1f + alpha,`transformOrigin` 设左侧防跳动。
- **主题引擎(三态)**: `ThemeSource = Preset(id) | FollowCover`,统一产出 ColorScheme。切换不重建 MaterialTheme,而是用 `AnimatedColorScheme` 对每个角色色做 `animateColorAsState(600ms)` 渐变(重组范围受限、性能安全)。DataStore 持久化。
- **共享元素转场**: `SharedTransitionLayout` + `Modifier.sharedElement(key="cover-$songId")`(列表封面↔播放页大封面),歌名/歌手文本用 `sharedBounds`。⚠️ Compose 1.7+ 可能仍带 `@ExperimentalSharedTransitionApi`,按 BOM 核实。
- **字体**: v1 用系统默认(国产 ROM 已"苹方化")或打包 MiSans(免费商用、SF 风);气质关键在**大字重 Bold/SemiBold + 大字号标题 + 大留白**,比换字体本身更重要。
- **迷你播放条↔播放页**: 用 `AnchoredDraggable` 做垂直拖拽展开,比做成两个页面简单且支持中途回弹。

**UI 落地顺序**: ①主题引擎(地基)→ ②播放页静态布局+模糊背景 → ③Pager 封面滑动切歌 → ④歌词滚动 → ⑤共享元素转场 → ⑥逐字歌词等精修(二期)。

---

## 五、工程规范(约束先行)

> 占位:CLAUDE.md 骨架 —— 目录结构、命名约定、模块边界、验证命令。正式实施第一步先建 CLAUDE.md。

---

## 六、验证方式(端到端)

> 你的红线:改完主动跑验证。本项目验证分三层,具体命令待 CLAUDE.md 定稿。

### 6.1 构建/静态验证
- `./gradlew assembleDebug` 编译通过;`./gradlew lint` 无阻断错误。
- NDK 库交叉编译通过(FFmpeg/libusb/soxr/taglib/dsd2pcm 全架构 arm64-v8a 起步)。

### 6.2 单元/解析验证(可自动化)
- LRC 解析:多时间戳行、元数据行、空行 → 行数与时间戳正确。
- 元数据:FLAC/APE/DSF/WAV 样本的标题/艺术家/封面提取正确。
- 输出协商:给定 DacCapability + 音轨格式 → OutputPlan 正确(尤其 DSD 的 Native/DoP/转PCM 优先级)。

### 6.3 真机功能验证(Android 15,需你配合插小尾巴)
- 媒体库:选定目录 → 自动增量扫描出新歌。
- 播放:FLAC / DSD(.dsf)/ APE / WAV 各播一首,系统通路出声正常、不爆音。
- **USB 独占(核心验收)**: 插小尾巴 → 开独占开关 → 播 DSD 与 FLAC,确认锁采样率(bit-perfect)、无重采样、无爆音;拔插切换通路状态一致。
- UI:封面模糊背景随封面变色、滚动歌词逐行高亮居中、预设主题切换平滑。
- **兼容矩阵(贯穿全程)**: 你的多条小尾巴逐一验证 claim / feedback 时钟 / DSD 模式。

### 6.4 一期"能日用"验收标准
插小尾巴开独占,连续播放一张 FLAC 专辑 + 一张 DSD 专辑,全程无爆音、无断连、UI 流畅、歌词滚动正常 —— 达成即一期成功。
