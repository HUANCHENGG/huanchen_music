// 播放引擎协调器实现(骨架版)
// 见 playback_engine.h 头注释。

#include "playback_engine.h"
#include <android/log.h>
#include <chrono>   // std::chrono::milliseconds(解码线程暂停休眠)
#include <thread>

#define LOG_TAG "PlaybackEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace huanchen {

void PlaybackEngine::setError(const std::string &msg) {
    mLastError = msg;
    mState.store(PlayState::ERROR);
    LOGE("%s", msg.c_str());
}

bool PlaybackEngine::load(const std::string &filePath) {
    // 加载前先停掉当前播放,保证状态干净
    stop();
    mState.store(PlayState::LOADING);

    // 1. 打开解码器,拿到文件格式
    PcmFormat fmt;
    if (!mDecoder.open(filePath, fmt)) {
        setError("解码失败: " + mDecoder.lastError());
        return false;
    }

    // 2. 按文件格式打开输出流
    if (!mOutput.open(fmt)) {
        setError("打开输出流失败");
        return false;
    }

    // 3. 记录格式描述供 UI 显示
    char desc[128];
    snprintf(desc, sizeof(desc), "%s · %dHz · %dch · %dbit%s",
             fmt.sourceDesc.c_str(), fmt.sampleRate, fmt.channels,
             fmt.bitsPerSample, fmt.isFloat ? " float" : "");
    mFormatDesc = desc;

    mState.store(PlayState::STOPPED);  // 加载完成,待播放
    LOGI("加载完成: %s", mFormatDesc.c_str());
    return true;
}

bool PlaybackEngine::play() {
    PlayState s = mState.load();
    if (s != PlayState::STOPPED && s != PlayState::PAUSED) {
        LOGI("当前状态 %d 不能 play", (int) s);
        return false;
    }

    // 启动输出流
    if (!mOutput.start()) {
        setError("启动输出流失败");
        return false;
    }
    mDecoding.store(true);

    // 解码线程只在没有运行时新建(暂停恢复直接复用)
    if (!mDecodeRunning.load()) {
        mDecodeRunning.store(true);
        mDecodeThread = std::thread(&PlaybackEngine::decodeLoop, this);
    }

    mState.store(PlayState::PLAYING);
    LOGI("开始播放");
    return true;
}

void PlaybackEngine::pause() {
    if (mState.load() != PlayState::PLAYING) return;
    mDecoding.store(false);     // 停喂数据(解码线程空转待命)
    mOutput.stop();             // 暂停输出流
    mState.store(PlayState::PAUSED);
    LOGI("已暂停");
}

void PlaybackEngine::stop() {
    // 停解码线程
    mDecodeRunning.store(false);
    mDecoding.store(false);
    if (mDecodeThread.joinable()) {
        mDecodeThread.join();
    }
    // 关输出与解码器
    mOutput.close();
    mDecoder.close();
    mState.store(PlayState::IDLE);
    LOGI("已停止");
}

void PlaybackEngine::decodeLoop() {
    LOGI("解码线程启动");
    std::vector<uint8_t> chunk;
    chunk.reserve(65536);

    while (mDecodeRunning.load()) {
        // 暂停时:不喂数据,短暂休眠,保持线程存活以便恢复
        if (!mDecoding.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        DecodeResult r = mDecoder.decodeChunk(chunk);
        if (r == DecodeResult::OK) {
            if (!chunk.empty()) {
                mOutput.write(chunk.data(), chunk.size());
            }
        } else if (r == DecodeResult::END_OF_STREAM) {
            LOGI("播放完成");
            mState.store(PlayState::STOPPED);
            mDecoding.store(false);
            break;  // 播完退出解码线程
        } else {
            setError("解码中途出错: " + mDecoder.lastError());
            break;
        }
    }
    LOGI("解码线程退出");
}

} // namespace huanchen
