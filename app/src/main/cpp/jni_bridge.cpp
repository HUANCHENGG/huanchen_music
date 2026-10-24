// JNI 桥接入口 —— Kotlin ↔ native 引擎层唯一通道(见 CLAUDE.md 第四节:跨层只走 bridge)
// 当前为最小骨架:验证 JNI 链路通,后续解码/输出/USB 独占都从这里注册 native 方法

#include <jni.h>
#include <android/log.h>

// 日志 tag,统一用 APP 名,方便 adb logcat 过滤
#define LOG_TAG "HuanchenEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// JNI 方法命名规则: Java_包名_类名_方法名(包名点号变下划线)
// 对应 Kotlin: com.huanchen.music.bridge.NativeBridge#nativeInit()
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeInit(JNIEnv *env, jobject /* thiz */) {
    // 引擎层初始化入口(占位):后续在这里创建解码器工厂/输出管理器等单例
    LOGI("native engine initialized");
    return env->NewStringUTF("huanchen_engine ready");
}
