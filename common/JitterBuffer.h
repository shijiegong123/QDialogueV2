/**
 * @file JitterBuffer.h
 * @brief 抖动缓冲器 - 解决电台网络抖动导致的音频爆音
 *
 * 设计原理：
 * - 接收到的 Opus 帧放入缓冲区，按序列号排序
 * - 播放端以固定速率 (20ms/帧) 从缓冲区取帧
 * - 缓冲深度自适应：网络抖动大时加深，抖动小时变浅
 * - 当缓冲区为空时，使用 PLC (丢包补偿) 填充静音/舒适噪声
 *
 * 缓冲策略：
 * - 最小缓冲: 2 帧 (40ms) - 最低延迟
 * - 最大缓冲: 10 帧 (200ms) - 最大容错
 * - 目标缓冲: 4 帧 (80ms) - 平衡延迟和容错
 */
#ifndef JITTERBUFFER_H
#define JITTERBUFFER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <QElapsedTimer>

class OpusCodec;

class JitterBuffer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param codec     Opus 编解码器 (用于 PLC 丢包补偿)
     * @param parent    父对象
     */
    explicit JitterBuffer(OpusCodec *codec, QObject *parent = nullptr);
    ~JitterBuffer();

    /**
     * @brief 启动缓冲器 (开始定时取帧)
     */
    void start();

    /**
     * @brief 停止缓冲器并清空数据
     */
    void stop();

    /**
     * @brief 放入一帧接收到的音频数据
     * @param opusData  Opus 编码数据
     * @param seqNum    序列号
     */
    void putFrame(const QByteArray &opusData, quint16 seqNum);

    /**
     * @brief 获取当前缓冲帧数
     */
    int bufferedFrames() const;

    /**
     * @brief 获取缓冲统计
     */
    struct Stats {
        quint32 totalPut      = 0;   // 总放入帧数
        quint32 totalGet      = 0;   // 总取出帧数
        quint32 plcFrames     = 0;   // PLC 补偿帧数
        quint32 lateDropped   = 0;   // 因太晚到达而丢弃的帧数
        quint32 bufferEmpty   = 0;   // 缓冲区为空次数
    };
    Stats getStats() const { return m_stats; }

signals:
    /**
     * @brief 取出一帧可供播放的 PCM 数据
     * @param pcmData PCM 数据 (16bit, mono, 16kHz)
     * @note  如果缓冲区空，pcmData 可能是 PLC 补偿数据或静音
     */
    void frameReady(const QByteArray &pcmData);

private slots:
    void onTick();         // 定时取帧
    void onAdaptDepth();   // 自适应调整缓冲深度

private:
    /**
     * @brief 从缓冲区取出一帧 (内部方法)
     * @return Opus 编码数据，如果缓冲区为空返回空 QByteArray
     */
    QByteArray takeFrame();

    /**
     * @brief 清理过期的帧 (太旧无法播放)
     */
    void cleanupStaleFrames();

    OpusCodec  *m_codec     = nullptr;
    QTimer     *m_tickTimer = nullptr;       // 20ms 定时取帧
    QTimer     *m_adaptTimer = nullptr;      // 自适应缓冲深度

    // 缓冲区: seqNum -> opusData
    QMap<quint16, QByteArray> m_buffer;
    mutable QMutex m_mutex;

    // 播放位置
    quint16 m_playSeqNum = 0;      // 下一个要播放的序列号
    bool    m_started    = false;  // 是否已开始播放

    // 自适应参数
    int m_minDepth    = 2;         // 最小缓冲帧数
    int m_maxDepth    = 10;        // 最大缓冲帧数
    int m_targetDepth = 4;         // 目标缓冲帧数 (自适应调整)

    // 统计
    Stats m_stats;

    // 用于自适应的时间戳
    QElapsedTimer m_elapsed;
    QList<int>    m_recentDelays;  // 最近的缓冲深度记录
};

#endif // JITTERBUFFER_H
