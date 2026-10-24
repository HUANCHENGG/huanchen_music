// AAudio 系统通路输出实现(骨架版)
// 见 aaudio_output.h 头注释。

#include "aaudio_output.h"
#include <android/log.h>
#include <cstring>
#include <thread>
#include <chrono>

#define LOG_TAG "AAudioOutput"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace huanchen {

// 环形缓冲容量:约 1 秒的 192kHz/32bit/立体声 PCM(留足余量)
// 192000 * 4 * 2 = 1.5MB/s,给 4MB 能撑 2 秒多,足够抗解码抖动
static constexpr size_t RING_BUFFER_BYTES = 4 * 1024 * 1024;

bool AAudioOutput::open(const PcmFormat &fmt) {
    close();
    mFormat = fmt;

    // 初始化环形缓冲
    mRingBuf.assign(RING_BUFFER_BYTES, 0);
    mReadPos = mWritePos = 0;
    mAvailable.store(0);

    // —— 配置 AAudio 流 ——
    AAudioStreamBuilder *builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder) {
        LOGE("创建 StreamBuilder 失败");
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, fmt.sampleRate);
    AAudioStreamBuilder_setChannelCount(builder, fmt.channels);
    // 采样格式:float 源用 FLOAT,否则按位深选 PCM
    aaudio_format_t aaFmt = fmt.isFloat ? AAUDIO_FORMAT_PCM_FLOAT
                                        : (fmt.bitsPerSample <= 16 ? AAUDIO_FORMAT_PCM_I16
                                                                   : AAUDIO_FORMAT_PCM_I32);
    AAudioStreamBuilder_setFormat(builder, aaFmt);

    // 性能模式:普通通路用 NONE(交给系统混音);独占才用 EXCLUSIVE(后续 UsbExclusiveOutput)
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);

    // 挂回调
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, this);

    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &mStream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !mStream) {
        LOGE("打开输出流失败: %s", AAudio_convertResultToText(result));
        mStream = nullptr;
        return false;
    }

    LOGI("输出流已打开: %dHz %dch fmt=%d(请求 %dHz %dch %dbit %s)",
         AAudioStream_getSampleRate(mStream),
         AAudioStream_getChannelCount(mStream),
         AAudioStream_getFormat(mStream),
         fmt.sampleRate, fmt.channels, fmt.bitsPerSample,
         fmt.isFloat ? "float" : "int");

    // 若系统给回的实际采样率与请求不符(SHARED 模式系统可能重采样),
    // 骨架版先记录告警,正式版由格式协商层决定是否重建
    if (AAudioStream_getSampleRate(mStream) != fmt.sampleRate) {
        LOGW("系统实际采样率 %d 与请求 %d 不符(系统将重采样)",
             AAudioStream_getSampleRate(mStream), fmt.sampleRate);
    }
    return true;
}

bool AAudioOutput::start() {
    if (!mStream) return false;
    aaudio_result_t r = AAudioStream_requestStart(mStream);
    if (r != AAUDIO_OK) {
        LOGE("启动流失败: %s", AAudio_convertResultToText(r));
        return false;
    }
    LOGI("输出流已启动");
    return true;
}

void AAudioOutput::stop() {
    if (mStream) {
        AAudioStream_requestPause(mStream);
        LOGI("输出流已暂停");
    }
}

void AAudioOutput::close() {
    if (mStream) {
        AAudioStream_requestStop(mStream);
        AAudioStream_close(mStream);
        mStream = nullptr;
        LOGI("输出流已关闭");
    }
    mRingBuf.clear();
    mAvailable.store(0);
}

size_t AAudioOutput::write(const uint8_t *data, size_t bytes) {
    if (mRingBuf.empty()) return 0;

    std::lock_guard<std::mutex> lock(mWriteMutex);
    size_t written = 0;
    while (written < bytes) {
        // 缓冲满了就等 callback 消费(简单自旋等待,骨架版够用)
        size_t avail = mAvailable.load();
        size_t freeSpace = RING_BUFFER_BYTES - avail;
        if (freeSpace == 0) {
            // 释放锁短暂让出,等输出线程消费
            // (骨架版用短暂睡眠;正式版可换条件变量)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        size_t chunk = std::min(bytes - written, freeSpace);
        // 环形写入(可能跨尾部回绕,分两段拷)
        for (size_t i = 0; i < chunk; ) {
            size_t contiguous = std::min(chunk - i, RING_BUFFER_BYTES - mWritePos);
            memcpy(mRingBuf.data() + mWritePos, data + written + i, contiguous);
            mWritePos = (mWritePos + contiguous) % RING_BUFFER_BYTES;
            i += contiguous;
        }
        written += chunk;
        mAvailable.fetch_add(chunk);
    }
    return written;
}

size_t AAudioOutput::bufferedBytes() const {
    return mAvailable.load();
}

// —— 静态回调转发到成员函数 ——
aaudio_data_callback_result_t AAudioOutput::dataCallback(AAudioStream * /*stream*/,
                                                         void *userData,
                                                         void *audioData,
                                                         int32_t numFrames) {
    auto *self = static_cast<AAudioOutput *>(userData);
    return self->onData(audioData, numFrames);
}

void AAudioOutput::errorCallback(AAudioStream * /*stream*/, void * /*userData*/,
                                 aaudio_result_t error) {
    LOGE("AAudio 流错误: %s", AAudio_convertResultToText(error));
    // 骨架版仅记录;正式版需处理流断开重建(如拔插设备)
}

aaudio_data_callback_result_t AAudioOutput::onData(void *audioData, int32_t numFrames) {
    // 计算本次回调需要的字节数
    int bytesPerSample = mFormat.isFloat ? 4 : (mFormat.bitsPerSample <= 16 ? 2 : 4);
    size_t needBytes = (size_t) numFrames * mFormat.channels * bytesPerSample;

    size_t avail = mAvailable.load();
    if (avail == 0) {
        // 欠载:没数据可给,填静音,避免杂音
        memset(audioData, 0, needBytes);
        if (!mUnderrun.exchange(true)) LOGW("输出欠载(缓冲空),输出静音");
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    mUnderrun.store(false);

    size_t toRead = std::min(needBytes, avail);
    // 环形读取(可能跨尾部回绕,分两段)
    uint8_t *dst = static_cast<uint8_t *>(audioData);
    for (size_t i = 0; i < toRead; ) {
        size_t contiguous = std::min(toRead - i, RING_BUFFER_BYTES - mReadPos);
        memcpy(dst + i, mRingBuf.data() + mReadPos, contiguous);
        mReadPos = (mReadPos + contiguous) % RING_BUFFER_BYTES;
        i += contiguous;
    }
    mAvailable.fetch_sub(toRead);

    // 若本次没填满(缓冲不足),剩余补静音
    if (toRead < needBytes) {
        memset(dst + toRead, 0, needBytes - toRead);
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

} // namespace huanchen
