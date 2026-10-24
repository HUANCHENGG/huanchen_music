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

#include "playback_engine.h"

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
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeGetFFmpegVersion(JNIEnv *env, jobject /* thiz */) {
    const char *version = av_version_info();
    char info[256];
    snprintf(info, sizeof(info),
             "FFmpeg %s | avcodec %u.%u.%u | avformat %u.%u.%u",
             version,
             LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO,
             LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    LOGI("ffmpeg linked: %s", info);
    return env->NewStringUTF(info);
}

// ============================================================
// 播放控制(骨架版:文件 → FFmpeg 解码 → AAudio 系统通路)
// ============================================================
using huanchen::PlaybackEngine;

// 加载音频文件。返回 true 成功;失败时可用 nativeGetLastError 取原因。
extern "C" JNIEXPORT jboolean JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeLoadFile(JNIEnv *env, jobject /* thiz */,
                                                           jstring filePath) {
    const char *path = env->GetStringUTFChars(filePath, nullptr);
    bool ok = PlaybackEngine::instance().load(path);
    env->ReleaseStringUTFChars(filePath, path);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// 开始/继续播放
extern "C" JNIEXPORT jboolean JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativePlay(JNIEnv * /*env*/, jobject /* thiz */) {
    return PlaybackEngine::instance().play() ? JNI_TRUE : JNI_FALSE;
}

// 暂停
extern "C" JNIEXPORT void JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativePause(JNIEnv * /*env*/, jobject /* thiz */) {
    PlaybackEngine::instance().pause();
}

// 停止(回到空闲)
extern "C" JNIEXPORT void JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeStop(JNIEnv * /*env*/, jobject /* thiz */) {
    PlaybackEngine::instance().stop();
}

// 查询当前播放状态(对应 PlayState 枚举序数):0空闲 1加载中 2播放中 3暂停 4已停 5错误
extern "C" JNIEXPORT jint JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeGetState(JNIEnv * /*env*/, jobject /* thiz */) {
    return static_cast<jint>(PlaybackEngine::instance().state());
}

// 当前文件格式描述(如 "FLAC · 96000Hz · 2ch · 24bit"),未加载返回空串
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeGetFormatDesc(JNIEnv *env, jobject /* thiz */) {
    return env->NewStringUTF(PlaybackEngine::instance().currentFormatDesc().c_str());
}

// 最近一次错误描述(无错误返回空串)
extern "C" JNIEXPORT jstring JNICALL
Java_com_huanchen_music_bridge_NativeBridge_nativeGetLastError(JNIEnv *env, jobject /* thiz */) {
    return env->NewStringUTF(PlaybackEngine::instance().lastError().c_str());
}
