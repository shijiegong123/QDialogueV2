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
    // 音频参数常量
    static constexpr int SAMPLE_RATE    = 16000;   // 采样率 16kHz
    static constexpr int CHANNELS       = 1;       // 单声道
    static constexpr int FRAME_DURATION = 20;      // 帧长 20ms
    static constexpr int FRAME_SAMPLES  = SAMPLE_RATE * FRAME_DURATION / 1000; // 320
    static constexpr int FRAME_PCM_SIZE = FRAME_SAMPLES * sizeof(qint16);      // 640 bytes
    static constexpr int MAX_PACKET_SIZE = 4000;   // 编码后最大包大小 (bytes)

    /**
     * @brief 构造函数
     * @param bitrate 编码比特率，默认 24000 bps (24kbps 足够语音)
     * @param parent  父对象
     */
    explicit OpusCodec(int bitrate = 24000, QObject *parent = nullptr);
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
