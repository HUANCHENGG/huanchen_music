package com.huanchen.music.bridge

/**
 * JNI 桥接 —— Kotlin 与 native 引擎层(C++)之间的唯一通道。
 *
 * 设计约定(见 CLAUDE.md 第四节):
 * - UI 不直接调用这里,必须经 player/audio 控制层。
 * - native 方法统一在此声明,不散落各处,便于维护 JNI 签名一致性。
 */
object NativeBridge {

    // 加载引擎层共享库(名字对应 cpp/CMakeLists.txt 的 add_library 名)
    init {
        System.loadLibrary("huanchen_engine")
    }

    /** 初始化 native 引擎,返回引擎状态字符串。 */
    external fun nativeInit(): String

    /** 获取 FFmpeg 版本信息,验证 FFmpeg 是否正确链接进引擎层。 */
    external fun nativeGetFFmpegVersion(): String

    // ============================================================
    // 播放控制(骨架版:文件 → FFmpeg 解码 → AAudio 系统通路)
    // ============================================================

    /** 加载音频文件。返回 true 成功;失败时用 nativeGetLastError() 取原因。 */
    external fun nativeLoadFile(filePath: String): Boolean

    /** 开始/继续播放。返回 false 表示当前状态不允许播放。 */
    external fun nativePlay(): Boolean

    /** 暂停播放。 */
    external fun nativePause()

    /** 停止播放,回到空闲态。 */
    external fun nativeStop()

    /**
     * 查询播放状态。返回值对应 PlayState 序数:
     * 0空闲 1加载中 2播放中 3暂停 4已停 5错误
     */
    external fun nativeGetState(): Int

    /** 当前文件格式描述(如 "FLAC · 96000Hz · 2ch · 24bit"),未加载返回空串。 */
    external fun nativeGetFormatDesc(): String

    /** 最近一次错误描述(无错误返回空串)。 */
    external fun nativeGetLastError(): String
}
