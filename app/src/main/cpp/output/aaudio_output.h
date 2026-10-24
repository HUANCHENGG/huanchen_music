// AAudio 系统通路输出 —— IAudioOutput 的第一个实现(普通系统混音通路)
//
// 职责:按指定 PCM 格式打开一条 AAudio 输出流,接收解码层喂来的 PCM 数据并播放。
//       这是「普通通路」(经系统混音器),非 USB 独占;独占走另一个实现 UsbExclusiveOutput。
//
// 线程模型:AAudio 用 data callback 在独立高优先级线程拉数据;
//          解码线程往 RingBuffer 塞数据,callback 从 RingBuffer 取。
//          本类内部用一个简单的环形缓冲做解耦。

#pragma once

#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include "../decoder/ffmpeg_decoder.h"

namespace huanchen {

// AAudio 输出流封装(骨架版:环形缓冲 + data callback 拉取)
class AAudioOutput {
public:
    AAudioOutput() = default;
    ~AAudioOutput() { close(); }

    AAudioOutput(const AAudioOutput &) = delete;
    AAudioOutput &operator=(const AAudioOutput &) = delete;

    // 打开输出流。fmt 为解码层报告的目标格式;失败返回 false。
    bool open(const PcmFormat &fmt);

    // 喂一块 PCM 数据进环形缓冲(解码线程调用)。阻塞等待缓冲有空间。
    // 返回实际写入字节数。
    size_t write(const uint8_t *data, size_t bytes);

    // 启动/暂停/关闭输出流。
    bool start();
    void stop();
    void close();

    // 当前环形缓冲已缓存的可读字节数(供 UI 显示缓冲水位)
    size_t bufferedBytes() const;

private:
    AAudioStream *mStream = nullptr;
    PcmFormat mFormat{};

    // —— 环形缓冲(解码线程写,callback 读) ——
    std::vector<uint8_t> mRingBuf;
    size_t mReadPos = 0;             // 读指针(仅 callback 线程动)
    size_t mWritePos = 0;            // 写指针(仅解码线程动)
    std::atomic<size_t> mAvailable{0}; // 可读字节数(原子,免锁)
    std::mutex mWriteMutex;          // 保护写指针推进
    std::atomic<bool> mUnderrun{false}; // 欠载标记(callback 取不到数据时置位)

    // AAudio data callback:输出线程每次要数据时回调,从环形缓冲取 PCM 填进 audioData
    static aaudio_data_callback_result_t dataCallback(AAudioStream *stream,
                                                      void *userData,
                                                      void *audioData,
                                                      int32_t numFrames);
    // AAudio error callback:流断开等错误时回调(如拔耳机)
    static void errorCallback(AAudioStream *stream, void *userData, aaudio_result_t error);

    aaudio_data_callback_result_t onData(void *audioData, int32_t numFrames);
};

} // namespace huanchen
