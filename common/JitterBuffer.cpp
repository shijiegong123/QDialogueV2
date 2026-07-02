/**
 * @file JitterBuffer.cpp
 * @brief 抖动缓冲器实现
 */
#include "JitterBuffer.h"
#include "OpusCodec.h"
#include <QDebug>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>

JitterBuffer::JitterBuffer(OpusCodec *codec, QObject *parent)
    : QObject(parent)
    , m_codec(codec)
{
}

JitterBuffer::~JitterBuffer()
{
    stop();
}

void JitterBuffer::start()
{
    if (m_started) return;

    m_started = true;
    m_playSeqNum = 0;
    m_stats = Stats();

    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
    m_recentDelays.clear();
    lock.unlock();

    // 每 20ms 取出一帧
    m_tickTimer = new QTimer(this);
    m_tickTimer->setTimerType(Qt::PreciseTimer);
    connect(m_tickTimer, &QTimer::timeout, this, &JitterBuffer::onTick);
    m_tickTimer->start(OpusCodec::FRAME_DURATION);  // 20ms

    // 每 2 秒自适应调整缓冲深度
    m_adaptTimer = new QTimer(this);
    connect(m_adaptTimer, &QTimer::timeout, this, &JitterBuffer::onAdaptDepth);
    m_adaptTimer->start(2000);

    m_elapsed.start();

    qDebug() << "[JitterBuffer] 启动 - 目标缓冲深度:" << m_targetDepth << "帧"
             << "(" << m_targetDepth * OpusCodec::FRAME_DURATION << "ms)";
}

void JitterBuffer::stop()
{
    m_started = false;

    if (m_tickTimer) {
        m_tickTimer->stop();
        m_tickTimer->deleteLater();
        m_tickTimer = nullptr;
    }

    if (m_adaptTimer) {
        m_adaptTimer->stop();
        m_adaptTimer->deleteLater();
        m_adaptTimer = nullptr;
    }

    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
    m_recentDelays.clear();

    qDebug() << "[JitterBuffer] 已停止 - 统计: 放入" << m_stats.totalPut
             << "取出" << m_stats.totalGet
             << "PLC" << m_stats.plcFrames
             << "丢弃" << m_stats.lateDropped;
}

void JitterBuffer::putFrame(const QByteArray &opusData, quint16 seqNum)
{
    QMutexLocker lock(&m_mutex);

    // 首次放入时，初始化播放序列号
    if (!m_started || m_buffer.isEmpty()) {
        m_playSeqNum = seqNum;
    }

    // 检查是否太旧 (落后播放位置超过一半的序列号空间)
    int diff = (seqNum - m_playSeqNum + 65536) % 65536;
    if (diff > 32768) {
        // 这是一个迟到的包，序列号远小于当前播放位置
        m_stats.lateDropped++;
        return;
    }

    // 检查是否超出最大缓冲范围
    if (diff > m_maxDepth * 2) {
        // 序列号跳跃太大，重置播放位置
        m_playSeqNum = seqNum;
        m_buffer.clear();
    }

    // 放入缓冲区
    if (!m_buffer.contains(seqNum)) {
        m_buffer.insert(seqNum, opusData);
        m_stats.totalPut++;
    }
}

void JitterBuffer::onTick()
{
    QByteArray opusFrame = takeFrame();
    QByteArray pcmData;

    if (opusFrame.isEmpty()) {
        // 缓冲区为空，使用 PLC 补偿
        if (m_codec && m_codec->isDecoderReady()) {
            pcmData = m_codec->decodePLC(1);
        } else {
            // 没有解码器，填充静音
            pcmData.fill(0, OpusCodec::FRAME_PCM_SIZE);
        }
        m_stats.plcFrames++;
        m_stats.bufferEmpty++;
    } else {
        // 正常解码
        if (m_codec && m_codec->isDecoderReady()) {
            pcmData = m_codec->decode(opusFrame);
        }
        m_stats.totalGet++;
    }

    if (!pcmData.isEmpty()) {
        emit frameReady(pcmData);
    }
}

QByteArray JitterBuffer::takeFrame()
{
    QMutexLocker lock(&m_mutex);

    if (m_buffer.isEmpty()) {
        // 缓冲区为空，推进播放序列号
        m_playSeqNum = (m_playSeqNum + 1) % 65536;
        return QByteArray();
    }

    // 尝试取出当前序列号的帧
    QByteArray frame;
    if (m_buffer.contains(m_playSeqNum)) {
        frame = m_buffer.take(m_playSeqNum);
    }

    // 推进播放位置
    m_playSeqNum = (m_playSeqNum + 1) % 65536;

    // 定期清理过期帧
    cleanupStaleFrames();

    // 记录当前缓冲深度 (用于自适应)
    m_recentDelays.append(m_buffer.size());

    return frame;
}

void JitterBuffer::cleanupStaleFrames()
{
    // 移除所有落后于播放位置的帧
    QList<quint16> toRemove;
    for (auto it = m_buffer.constBegin(); it != m_buffer.constEnd(); ++it) {
        int diff = (m_playSeqNum - it.key() + 65536) % 65536;
        if (diff > 0 && diff < 32768) {
            // 这个帧已经过期
            toRemove.append(it.key());
        }
    }

    for (quint16 seq : toRemove) {
        m_buffer.remove(seq);
        m_stats.lateDropped++;
    }
}

void JitterBuffer::onAdaptDepth()
{
    QMutexLocker lock(&m_mutex);

    if (m_recentDelays.isEmpty()) {
        return;
    }

    // 计算最近 2 秒内的平均缓冲深度和最大值
    double avgDepth = 0;
    int maxDepth = 0;
    for (int d : m_recentDelays) {
        avgDepth += d;
        if (d > maxDepth) maxDepth = d;
    }
    avgDepth /= m_recentDelays.size();
    m_recentDelays.clear();
    lock.unlock();

    // 自适应策略：
    // - 如果最大深度 > 当前目标的 2 倍，增加目标
    // - 如果平均深度 < 当前目标的 0.5 倍，减少目标
    int newTarget = m_targetDepth;

    if (maxDepth > m_targetDepth * 2) {
        // 网络抖动大，增加缓冲
        newTarget = qMin(m_targetDepth + 1, m_maxDepth);
    } else if (avgDepth < m_targetDepth * 0.5 && m_stats.bufferEmpty < 5) {
        // 网络稳定，减少缓冲以降低延迟
        newTarget = qMax(m_targetDepth - 1, m_minDepth);
    }

    if (newTarget != m_targetDepth) {
        m_targetDepth = newTarget;
        qDebug() << "[JitterBuffer] 自适应调整 - 目标缓冲深度:"
                 << m_targetDepth << "帧"
                 << "平均:" << avgDepth << "最大:" << maxDepth;
    }
}

int JitterBuffer::bufferedFrames() const
{
    QMutexLocker lock(&m_mutex);
    return m_buffer.size();
}
