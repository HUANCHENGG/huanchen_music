// JNI 桥接入口 —— Kotlin ↔ native 引擎层唯一通道(见 CLAUDE.md 第四节:跨层只走 bridge)
// 当前:验证 JNI 链路 + FFmpeg 链接可用,后续解码/输出/USB 独占都从这里注册 native 方法

#include <jni.h>
#include <android/log.h>
#include <string>

// FFmpeg 头文件(extern "C" 包裹:FFmpeg 是 C 库,直接 #include 会因 C++ name mangling 找不到符号)
extern "C" {
#include <libavutil/avutil.h>   // av_version_info()
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// 日志 tag,统一用 APP 名,方便 adb logcat 过滤
#define LOG_TAG "HuanchenEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 对应 Kotlin: com.huanchen.music.bridge.NativeBridge#nativeInit()
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeInit(JNIEnv *env, jobject /* thiz */) {
    LOGI("native engine initialized");
    return env->NewStringUTF("huanchen_engine ready");
}

// 对应 Kotlin: com.huanchen.music.bridge.NativeBridge#nativeGetFFmpegVersion()
// 验证 FFmpeg 是否正确链接:调用 av_version_info() 返回版本串。
// 若链接失败,APP 启动加载 huanchen_engine 时会 UnsatisfiedLinkError。
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeGetFFmpegVersion(JNIEnv *env, jobject /* thiz */) {
    // av_version_info() 返回 FFmpeg 版本字符串(如 "7.1")
    const char *version = av_version_info();
    // 顺带取编解码器/格式版本,确认三个库都链接上了
    char info[256];
    snprintf(info, sizeof(info),
             "FFmpeg %s | avcodec %u.%u.%u | avformat %u.%u.%u",
             version,
             LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO,
             LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    LOGI("ffmpeg linked: %s", info);
    return env->NewStringUTF(info);
}
