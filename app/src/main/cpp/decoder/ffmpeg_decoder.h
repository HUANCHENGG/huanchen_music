// FFmpeg 解码器 —— 引擎层解码核心(IDecoder 的最小实现)
//
// 职责:打开一个本地音频文件(任意 FFmpeg 支持的格式:FLAC/APE/DSF/DFF/WAV/MP3...),
//       解复用 + 解码,输出统一的 PCM 数据块,并报告该文件的音频格式(采样率/声道/位深)。
//
// 设计约定(见 CLAUDE.md 第四节):
//   - 解码层不知道输出走哪条通路,只负责产出 PCM + 报告格式;
//   - 格式协商由控制层在打开输出前完成(本骨架版直接由解码层报告格式,交由输出层尝试)。
//
// 线程安全:本类非线程安全,所有方法需在解码线程内调用。

#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace huanchen {

// 解码出的 PCM 格式描述(交给输出层做格式协商的依据)
struct PcmFormat {
    int sampleRate = 0;      // 采样率(Hz),如 44100 / 96000 / 192000 / DSD 转 PCM 后的目标率
    int channels = 0;        // 声道数,如 2
    int bitsPerSample = 0;   // 位深,如 16 / 24 / 32
    bool isFloat = false;    // 是否浮点采样
    // 原始来源描述(用于 UI 显示,如 "FLAC 96kHz/24bit")
    std::string sourceDesc;
};

// 解码结果状态
enum class DecodeResult {
    OK,             // 成功解出一个数据块
    END_OF_STREAM,  // 文件读完
    ERROR           // 出错(详见 lastError)
};

class FFmpegDecoder {
public:
    FFmpegDecoder() = default;
    ~FFmpegDecoder() { close(); }

    // 禁止拷贝(持有 FFmpeg 上下文,拷贝会双重释放)
    FFmpegDecoder(const FFmpegDecoder &) = delete;
    FFmpegDecoder &operator=(const FFmpegDecoder &) = delete;

    // 打开文件并定位第一个音频流,初始化解码器与重采样器。
    // 成功返回 true 并填充 outFormat;失败返回 false 并写 lastError。
    bool open(const std::string &filePath, PcmFormat &outFormat);

    // 解码下一块 PCM 数据,追加到 outBuffer(交错采样,interleaved)。
    // 返回 DecodeResult;outBuffer 内容仅在 OK 时有效。
    DecodeResult decodeChunk(std::vector<uint8_t> &outBuffer, size_t maxBytes = 16384);

    // 关闭并释放所有 FFmpeg 资源。
    void close();

    // 最近一次错误的可读描述(供 JNI 上报给 UI)
    const std::string &lastError() const { return mLastError; }

private:
    AVFormatContext *mFormatCtx = nullptr;  // 解复用上下文
    AVCodecContext *mCodecCtx = nullptr;    // 解码器上下文
    SwrContext *mSwrCtx = nullptr;          // 重采样上下文(统一输出格式)
    AVPacket *mPacket = nullptr;            // 复用的压缩包
    AVFrame *mFrame = nullptr;              // 复用的解码帧
    int mAudioStreamIndex = -1;             // 选中的音频流下标
    std::string mLastError;

    // 输出目标格式:统一转成 交错 + 指定采样格式,简化输出层
    // 位深保留源精度(HiFi 关键),仅统一声道布局与打包方式
    AVChannelLayout mOutLayout{};
    // 实际输出格式(重采样后,位深可能被统一到容器位深,如 24bit→32bit)
    PcmFormat mOutFormat_{};
    AVSampleFormat mOutSampleFmt_ = AV_SAMPLE_FMT_NONE;

    bool setupResampler(const PcmFormat &fmt);
    void setError(const std::string &msg);
};

} // namespace huanchen
