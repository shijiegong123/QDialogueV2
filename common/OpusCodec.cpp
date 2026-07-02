/**
 * @file OpusCodec.cpp
 * @brief Opus 编解码器实现
 */
#include "OpusCodec.h"
#include <QDebug>
#include <cstring>

OpusCodec::OpusCodec(int bitrate, QObject *parent)
    : QObject(parent)
    , m_bitrate(bitrate)
{
}

OpusCodec::~OpusCodec()
{
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
    if (m_decoder) {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }
}

bool OpusCodec::initialize()
{
    if (m_initialized) {
        return true;
    }

    int err = 0;

    // ---- 创建编码器 ----
    // 使用 VOIP 模式优化语音，低延迟
    m_encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS,
                                     OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !m_encoder) {
        qCritical() << "[OpusCodec] 编码器创建失败:" << opus_strerror(err);
        return false;
    }

    // 设置比特率
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(m_bitrate));
    // 使用 20ms 帧长
    opus_encoder_ctl(m_encoder, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
    // 开启 DTX (静音时降低比特率，内置 VAD 判断) 0：关闭 DTX（持续发包） 1：开启 DTX（静音时减少发包）
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(1));
    // 设置信号类型为语音
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    // 设置复杂度 (RK3588 算力足够，设为中等)
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));
    // 设置最大带宽为宽带 (8kHz)
    opus_encoder_ctl(m_encoder, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
    // 禁用 FEC (前向纠错)，我们用 JitterBuffer + PLC 处理丢包
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(0));
    // 预期丢包率 0%
    opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(0));

    // ---- 创建解码器 ----
    m_decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK || !m_decoder) {
        qCritical() << "[OpusCodec] 解码器创建失败:" << opus_strerror(err);
        if (m_encoder) {
            opus_encoder_destroy(m_encoder);
            m_encoder = nullptr;
        }
        return false;
    }

    m_initialized = true;
    qDebug() << "[OpusCodec] 初始化成功 - 采样率:" << SAMPLE_RATE
             << "Hz, 帧长:" << FRAME_DURATION << "ms, 比特率:" << m_bitrate << "bps";

    return true;
}

QByteArray OpusCodec::encode(const QByteArray &pcmData)
{
    if (!m_encoder) {
        qWarning() << "[OpusCodec] 编码器未初始化";
        return QByteArray();
    }

    // 检查 PCM 数据长度是否为一帧
    if (pcmData.size() != FRAME_PCM_SIZE) {
        qWarning() << "[OpusCodec] PCM 数据长度错误: 期望"
                    << FRAME_PCM_SIZE << "字节, 实际" << pcmData.size() << "字节";
        return QByteArray();
    }

    // 将 QByteArray 转换为 opus_int16 采样点数组
    const opus_int16 *samples = reinterpret_cast<const opus_int16 *>(pcmData.constData());

    // 编码
    unsigned char output[MAX_PACKET_SIZE];
    int encodedLen = opus_encode(m_encoder, samples, FRAME_SAMPLES,
                                  output, MAX_PACKET_SIZE);
    if (encodedLen < 0) {
        qWarning() << "[OpusCodec] 编码失败:" << opus_strerror(encodedLen);
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(output), encodedLen);
}

QByteArray OpusCodec::encode(const QVector<qint16> &samples)
{
    if (!m_encoder) {
        qWarning() << "[OpusCodec] 编码器未初始化";
        return QByteArray();
    }

    if (samples.size() != FRAME_SAMPLES) {
        qWarning() << "[OpusCodec] 采样点数量错误: 期望"
                    << FRAME_SAMPLES << ", 实际" << samples.size();
        return QByteArray();
    }

    unsigned char output[MAX_PACKET_SIZE];
    int encodedLen = opus_encode(m_encoder, samples.constData(), FRAME_SAMPLES,
                                  output, MAX_PACKET_SIZE);
    if (encodedLen < 0) {
        qWarning() << "[OpusCodec] 编码失败:" << opus_strerror(encodedLen);
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(output), encodedLen);
}

QByteArray OpusCodec::decode(const QByteArray &opusData)
{
    if (!m_decoder) {
        qWarning() << "[OpusCodec] 解码器未初始化";
        return QByteArray();
    }

    if (opusData.isEmpty()) {
        return QByteArray();
    }

    // 分配解码缓冲区
    opus_int16 pcmBuffer[FRAME_SAMPLES];

    int decodedSamples = opus_decode(
        m_decoder,
        reinterpret_cast<const unsigned char *>(opusData.constData()),
        opusData.size(),
        pcmBuffer,
        FRAME_SAMPLES,
        0  // 不使用 FEC
    );

    if (decodedSamples < 0) {
        qWarning() << "[OpusCodec] 解码失败:" << opus_strerror(decodedSamples);
        return QByteArray();
    }

    // 将解码后的 PCM 数据转换为 QByteArray
    return QByteArray(reinterpret_cast<const char *>(pcmBuffer),
                      decodedSamples * sizeof(opus_int16));
}

QVector<qint16> OpusCodec::decodeToSamples(const QByteArray &opusData)
{
    QVector<qint16> result;

    if (!m_decoder || opusData.isEmpty()) {
        return result;
    }

    result.resize(FRAME_SAMPLES);

    int decodedSamples = opus_decode(
        m_decoder,
        reinterpret_cast<const unsigned char *>(opusData.constData()),
        opusData.size(),
        result.data(),
        FRAME_SAMPLES,
        0
    );

    if (decodedSamples < 0) {
        qWarning() << "[OpusCodec] 解码失败:" << opus_strerror(decodedSamples);
        result.clear();
    } else {
        result.resize(decodedSamples);
    }

    return result;
}

QByteArray OpusCodec::decodePLC(int numLostPackets)
{
    if (!m_decoder) {
        return QByteArray();
    }

    QByteArray result;
    result.reserve(FRAME_PCM_SIZE * numLostPackets);

    for (int i = 0; i < numLostPackets; ++i) {
        opus_int16 pcmBuffer[FRAME_SAMPLES];

        // 传入 nullptr 数据触发 PLC (丢包补偿)
        int decodedSamples = opus_decode(m_decoder, nullptr, 0,
                                          pcmBuffer, FRAME_SAMPLES, 0);
        if (decodedSamples < 0) {
            qWarning() << "[OpusCodec] PLC 解码失败:" << opus_strerror(decodedSamples);
            break;
        }

        result.append(reinterpret_cast<const char *>(pcmBuffer),
                      decodedSamples * sizeof(opus_int16));
    }

    return result;
}

void OpusCodec::setBitrate(int bitrate)
{
    if (m_encoder && bitrate > 0) {
        m_bitrate = bitrate;
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
        qDebug() << "[OpusCodec] 比特率已调整为" << bitrate << "bps";
    }
}
