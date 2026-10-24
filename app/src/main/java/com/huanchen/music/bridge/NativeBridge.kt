package com.huanchen.music.bridge

/**
 * JNI 桥接 —— Kotlin 与 native 引擎层(C++)之间的唯一通道。
 *
 * 设计约定(见 CLAUDE.md 第四节):
 * - UI 不直接调用这里,必须经 player/audio 控制层。
 * - native 方法统一在此声明,不散落各处,便于维护 JNI 签名一致性。
 *
 * 当前为最小骨架:验证 JNI 链路可用,后续解码/输出/USB 独占在此扩展。
 */
object NativeBridge {

    // 加载引擎层共享库(名字对应 cpp/CMakeLists.txt 的 add_library 名)
    init {
        System.loadLibrary("huanchen_engine")
    }

    /**
     * 初始化 native 引擎,返回引擎状态字符串。
     * 对应 cpp/jni_bridge.cpp 的 Java_com_huanchen_music_bridge_NativeBridge_nativeInit。
     */
    external fun nativeInit(): String
}
