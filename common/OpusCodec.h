/**
 * @file OpusCodec.h
 * @brief Opus 编解码器 C++ 封装 (基于 libopus)
 *
 * 针对 RK3588 嵌入式平台优化，使用 20ms 帧长，
 * 采样率 16kHz，单声道，16bit PCM。
 * 每帧 320 个采样点 = 640 字节 PCM 数据。
 */
#ifndef OPUSCODEC_H
#define OPUSCODEC_H

#include <QObject>
#include <QByteArray>
#include <QVector>
#include <opus/opus.h>

class OpusCodec : public QObject
{
    Q_OBJECT

public:
    //RK3588 的 ES8388 驱动在某些 BSP 版本下，默认只支持 48kHz 或 44.1kHz。
    //当您请求 16kHz 时，Qt 可能会默默回退（Fallback）到一个不支持的格式，或者 ALSA 底层在做重采样（plughw），
    //导致实际的数据吞吐速率与您计算的 FRAME_PCM_SIZE 完全错位

    // 音频参数常量
    //由于 RK3588 的 ES8388 硬件特性（强制 48000Hz 和双声道）
    //【修改 1】采样率从 16000 改为 48000，匹配 ES8388 硬件
    static constexpr int SAMPLE_RATE    = 48000;   // 采样率 48kHz
    // 【修改 2】通道数从1改为 2 (双声道)，匹配 ES8388 硬件
    static constexpr int CHANNELS       = 2;       // 声道
    static constexpr int FRAME_DURATION = 20;      // 帧长 20ms
    static constexpr int FRAME_SAMPLES  = SAMPLE_RATE * FRAME_DURATION / 1000; // 960
    // 960 采样点 * 2 通道 * 2 字节(16bit) = 3840 bytes
    static constexpr int FRAME_PCM_SIZE = FRAME_SAMPLES * CHANNELS * sizeof(qint16);
    static constexpr int MAX_PACKET_SIZE = 5000;   // 编码后最大包大小 (bytes)

    /**
     * @brief 构造函数
     * @param bitrate 编码比特率，默认 48000 bps (48kbps 足够语音)
     * @param parent  父对象
     */
    explicit OpusCodec(int bitrate = 48000, QObject *parent = nullptr);
    ~OpusCodec();

    /**
     * @brief 初始化编码器和解码器
     * @return 初始化成功返回 true
     */
    bool initialize();

    /**
     * @brief 编码一帧 PCM 数据为 Opus
     * @param pcmData  原始 PCM 数据 (16bit signed, mono, 16kHz)
     *                 长度必须为 FRAME_PCM_SIZE (640 bytes)
     * @return 编码后的 Opus 数据包，失败返回空 QByteArray
     */
    QByteArray encode(const QByteArray &pcmData);

    /**
     * @brief 编码一帧 PCM 数据 (采样点数组)
     * @param samples  PCM 采样点数组，长度必须为 FRAME_SAMPLES
     * @return 编码后的 Opus 数据包
     */
    QByteArray encode(const QVector<qint16> &samples);

    /**
     * @brief 解码一帧 Opus 数据为 PCM
     * @param opusData  Opus 编码的数据包
     * @return 解码后的 PCM 数据 (16bit signed, mono, 16kHz)
     */
    QByteArray decode(const QByteArray &opusData);

    /**
     * @brief 解码并返回采样点数组
     * @param opusData Opus 编码的数据包
     * @return PCM 采样点数组
     */
    QVector<qint16> decodeToSamples(const QByteArray &opusData);

    /**
     * @brief PLC 丢包补偿 - 当检测到丢包时生成舒适噪声
     * @param numLostPackets 丢失的包数
     * @return 补偿的 PCM 数据
     */
    QByteArray decodePLC(int numLostPackets = 1);

    /**
     * @brief 检查编码器是否就绪
     */
    bool isEncoderReady() const { return m_encoder != nullptr; }

    /**
     * @brief 检查解码器是否就绪
     */
    bool isDecoderReady() const { return m_decoder != nullptr; }

    /**
     * @brief 动态调整编码比特率
     * @param bitrate 新的比特率 (bps)
     */
    void setBitrate(int bitrate);

private:
    OpusEncoder *m_encoder = nullptr;
    OpusDecoder *m_decoder = nullptr;
    int          m_bitrate;
    bool         m_initialized = false;
};

#endif // OPUSCODEC_H
