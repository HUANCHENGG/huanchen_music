// FFmpeg 解码器实现(骨架版)
// 见 ffmpeg_decoder.h 头注释。目标:可靠地把本地音频文件解码成交错 PCM。

#include "ffmpeg_decoder.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "FFmpegDecoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace huanchen {

void FFmpegDecoder::setError(const std::string &msg) {
    mLastError = msg;
    LOGE("%s", msg.c_str());
}

bool FFmpegDecoder::open(const std::string &filePath, PcmFormat &outFormat) {
    close();  // 幂等:重复 open 先释放旧资源

    // 1. 打开文件并探测格式(demux)
    //    avformat_open_input 会自动选择正确的 demuxer(flac/ape/dsf/wav...)
    int ret = avformat_open_input(&mFormatCtx, filePath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        setError("打开文件失败: " + std::string(errbuf));
        return false;
    }

    // 2. 读取流信息(填充各流的 codecpar)
    if (avformat_find_stream_info(mFormatCtx, nullptr) < 0) {
        setError("读取流信息失败");
        return false;
    }

    // 3. 找第一个音频流
    mAudioStreamIndex = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_AUDIO,
                                            -1, -1, nullptr, 0);
    if (mAudioStreamIndex < 0) {
        setError("文件中没有音频流");
        return false;
    }
    AVStream *stream = mFormatCtx->streams[mAudioStreamIndex];
    AVCodecParameters *codecpar = stream->codecpar;

    // 4. 找解码器并创建解码上下文
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        setError("找不到解码器: " + std::string(avcodec_get_name(codecpar->codec_id)));
        return false;
    }
    mCodecCtx = avcodec_alloc_context3(codec);
    if (!mCodecCtx) {
        setError("创建解码上下文失败");
        return false;
    }
    if (avcodec_parameters_to_context(mCodecCtx, codecpar) < 0) {
        setError("填充解码参数失败");
        return false;
    }
    if (avcodec_open2(mCodecCtx, codec, nullptr) < 0) {
        setError("打开解码器失败");
        return false;
    }

    // 5. 分配复用的 packet / frame
    mPacket = av_packet_alloc();
    mFrame = av_frame_alloc();
    if (!mPacket || !mFrame) {
        setError("分配 packet/frame 失败");
        return false;
    }

    // 6. 填充输出格式描述
    outFormat.sampleRate = mCodecCtx->sample_rate;
    outFormat.channels = mCodecCtx->ch_layout.nb_channels;
    // 位深与是否浮点由采样格式决定
    AVSampleFormat fmt = mCodecCtx->sample_fmt;
    outFormat.isFloat = av_sample_fmt_is_planar(fmt) ? (fmt == AV_SAMPLE_FMT_FLTP || fmt == AV_SAMPLE_FMT_DBLP)
                                                     : (fmt == AV_SAMPLE_FMT_FLT || fmt == AV_SAMPLE_FMT_DBL);
    outFormat.bitsPerSample = av_get_bytes_per_sample(fmt) * 8;
    outFormat.sourceDesc = std::string(codec->long_name ? codec->long_name : codec->name);

    // 7. 配置重采样器:统一输出为 交错(packed)+ 保留源采样率/位深
    //    目的:输出层只需处理一种打包方式,降低复杂度;位深不动以保精度(HiFi)
    if (!setupResampler(outFormat)) {
        return false;  // setupResampler 内部已 setError
    }

    LOGI("打开成功: %s | %s %dHz %dch %dbit %s",
         filePath.c_str(), outFormat.sourceDesc.c_str(),
         outFormat.sampleRate, outFormat.channels, outFormat.bitsPerSample,
         outFormat.isFloat ? "float" : "int");
    return true;
}

bool FFmpegDecoder::setupResampler(const PcmFormat &fmt) {
    // 目标采样格式:统一交错。位深对齐源(16→S16, 24/32→S32, float→FLT)
    // 说明:AAudio 输出对 S16/FLT 支持最好;24bit 源提升到 32bit int 容器保精度。
    AVSampleFormat outFmt;
    if (fmt.isFloat) {
        outFmt = AV_SAMPLE_FMT_FLT;
    } else if (fmt.bitsPerSample <= 16) {
        outFmt = AV_SAMPLE_FMT_S16;
    } else {
        outFmt = AV_SAMPLE_FMT_S32;  // 24bit 源装进 32bit 容器
    }

    // 输出声道布局 = 源布局(不改声道)
    if (av_channel_layout_copy(&mOutLayout, &mCodecCtx->ch_layout) < 0) {
        setError("复制声道布局失败");
        return false;
    }

    int ret = swr_alloc_set_opts2(&mSwrCtx,
                                  &mOutLayout, outFmt, fmt.sampleRate,   // 输出
                                  &mCodecCtx->ch_layout, mCodecCtx->sample_fmt,
                                  mCodecCtx->sample_rate,                 // 输入
                                  0, nullptr);
    if (ret < 0 || !mSwrCtx) {
        setError("创建重采样器失败");
        return false;
    }
    if (swr_init(mSwrCtx) < 0) {
        setError("初始化重采样器失败");
        return false;
    }

    // 回填实际输出格式(可能被统一到 32bit 容器)
    mOutFormat_ = fmt;
    mOutFormat_.bitsPerSample = av_get_bytes_per_sample(outFmt) * 8;
    mOutFormat_.isFloat = (outFmt == AV_SAMPLE_FMT_FLT);
    mOutSampleFmt_ = outFmt;
    return true;
}

DecodeResult FFmpegDecoder::decodeChunk(std::vector<uint8_t> &outBuffer, size_t maxBytes) {
    outBuffer.clear();
    if (!mFormatCtx || !mCodecCtx || !mSwrCtx) {
        setError("解码器未初始化");
        return DecodeResult::ERROR;
    }

    const int bytesPerSample = av_get_bytes_per_sample(mOutSampleFmt_);
    const int outChannels = mOutLayout.nb_channels;

    // 循环读包→送解码→收帧,直到凑够一块或到文件尾
    while (outBuffer.size() < maxBytes) {
        int ret = av_read_frame(mFormatCtx, mPacket);
        if (ret == AVERROR_EOF) {
            // 文件尾:冲刷解码器内残余帧
            avcodec_send_packet(mCodecCtx, nullptr);
            if (avcodec_receive_frame(mCodecCtx, mFrame) == 0) {
                // 还有残余帧,继续走下面的转换逻辑
            } else {
                return outBuffer.empty() ? DecodeResult::END_OF_STREAM : DecodeResult::OK;
            }
        } else if (ret < 0) {
            setError("读取数据包失败");
            return DecodeResult::ERROR;
        } else {
            // 只处理音频流的包
            if (mPacket->stream_index != mAudioStreamIndex) {
                av_packet_unref(mPacket);
                continue;
            }
            if (avcodec_send_packet(mCodecCtx, mPacket) < 0) {
                av_packet_unref(mPacket);
                setError("送包给解码器失败");
                return DecodeResult::ERROR;
            }
            av_packet_unref(mPacket);
            if (avcodec_receive_frame(mCodecCtx, mFrame) < 0) {
                // 解码器需要更多包才能出一帧,继续读
                continue;
            }
        }

        // —— 把这一帧重采样/转换成统一输出格式 ——
        // 估算输出样本数(含重采样延迟)
        int outSamples = swr_get_out_samples(mSwrCtx, mFrame->nb_samples);
        if (outSamples < 0) outSamples = mFrame->nb_samples;

        size_t offset = outBuffer.size();
        outBuffer.resize(offset + (size_t) outSamples * outChannels * bytesPerSample);
        uint8_t *outPlanes[1] = {outBuffer.data() + offset};

        int converted = swr_convert(mSwrCtx,
                                    outPlanes, outSamples,
                                    (const uint8_t **) mFrame->extended_data,
                                    mFrame->nb_samples);
        av_frame_unref(mFrame);

        if (converted < 0) {
            setError("重采样转换失败");
            return DecodeResult::ERROR;
        }
        // 按实际转换出的样本数收缩 buffer(估算值可能偏大)
        outBuffer.resize(offset + (size_t) converted * outChannels * bytesPerSample);
    }

    return DecodeResult::OK;
}

void FFmpegDecoder::close() {
    if (mSwrCtx) { swr_free(&mSwrCtx); mSwrCtx = nullptr; }
    if (mFrame) { av_frame_free(&mFrame); mFrame = nullptr; }
    if (mPacket) { av_packet_free(&mPacket); mPacket = nullptr; }
    if (mCodecCtx) { avcodec_free_context(&mCodecCtx); mCodecCtx = nullptr; }
    if (mFormatCtx) { avformat_close_input(&mFormatCtx); mFormatCtx = nullptr; }
    mAudioStreamIndex = -1;
    av_channel_layout_uninit(&mOutLayout);
}

} // namespace huanchen
