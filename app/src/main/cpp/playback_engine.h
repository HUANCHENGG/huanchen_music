// 播放引擎协调器 —— 引擎层核心,串联解码器与输出,管理解码线程
//
// 职责:
//   - 持有一个 FFmpegDecoder + 一个 AAudioOutput(骨架版仅系统通路)
//   - 起一个解码线程:循环 decodeChunk → 喂给输出层的环形缓冲
//   - 对 JNI 暴露:加载文件 / 播放 / 暂停 / 停止 / 查询状态
//
// 线程模型:
//   - 解码线程(普通优先级):FFmpegDecoder.decodeChunk → AAudioOutput.write
//   - 输出线程(AAudio 回调,高优先级):从环形缓冲取数据
//   两者经 AAudioOutput 内的环形缓冲解耦。

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include "decoder/ffmpeg_decoder.h"
#include "output/aaudio_output.h"

namespace huanchen {

// 播放状态(映射给 Kotlin UI 显示)
enum class PlayState {
    IDLE,       // 空闲(未加载)
    LOADING,    // 加载中
    PLAYING,    // 播放中
    PAUSED,     // 已暂停
    STOPPED,    // 已停止/播完
    ERROR       // 出错
};

class PlaybackEngine {
public:
    // 全局单例(JNI 层持有,骨架版单实例即可)
    static PlaybackEngine &instance() {
        static PlaybackEngine s;
        return s;
    }

    // 加载文件:打开解码器 + 按文件格式打开输出流。成功返回 true。
    bool load(const std::string &filePath);

    // 播放控制
    bool play();    // 启动输出流 + 解码线程
    void pause();   // 暂停输出流(解码线程挂起)
    void stop();    // 停止并回到 IDLE

    // 状态查询(供 JNI 上报 UI)
    PlayState state() const { return mState.load(); }
    std::string currentFormatDesc() const { return mFormatDesc; }
    size_t bufferedBytes() const { return mOutput.bufferedBytes(); }
    const std::string &lastError() const { return mLastError; }

private:
    PlaybackEngine() = default;
    ~PlaybackEngine() { stop(); }
    PlaybackEngine(const PlaybackEngine &) = delete;
    PlaybackEngine &operator=(const PlaybackEngine &) = delete;

    void decodeLoop();  // 解码线程主循环
    void setError(const std::string &msg);

    FFmpegDecoder mDecoder;
    AAudioOutput mOutput;

    std::thread mDecodeThread;
    std::atomic<bool> mDecodeRunning{false};  // 解码线程是否应继续
    std::atomic<bool> mDecoding{false};       // 是否在主动喂数据(暂停时停喂)
    std::atomic<PlayState> mState{PlayState::IDLE};
    std::string mFormatDesc;
    std::string mLastError;
};

} // namespace huanchen
