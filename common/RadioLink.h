/**
 * @file RadioLink.h
 * @brief UDP 网络传输层 - 适配电台弱网环境
 *
 * 功能：
 * - 基于 QUdpSocket 的可靠 UDP 传输
 * - 自动维护序列号 (检测丢包和乱序)
 * - 包头序列化/反序列化
 * - 心跳保活机制
 */
#ifndef RADIOLINK_H
#define RADIOLINK_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include "PTTProtocol.h"

class RadioLink : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param localPort   本地监听端口
     * @param peerAddr    对端 IP 地址
     * @param peerPort    对端端口
     * @param parent      父对象
     */
    explicit RadioLink(quint16 localPort,
                       const QString &peerAddr,
                       quint16 peerPort,
                       QObject *parent = nullptr);
    ~RadioLink();

    /**
     * @brief 启动网络层
     * @return 绑定端口成功返回 true
     */
    bool start();

    /**
     * @brief 停止网络层
     */
    void stop();

    /**
     * @brief 发送一帧音频数据
     * @param opusPayload Opus 编码后的数据
     */
    void sendAudioFrame(const QByteArray &opusPayload);

    /**
     * @brief 发送 PTT 信令
     * @param type 信令类型 (PTT_REQUEST / PTT_GRANT / PTT_DENY / PTT_RELEASE)
     */
    void sendSignaling(FrameType type);

    /**
     * @brief 获取当前序列号 (用于调试)
     */
    quint16 currentSeqNum() const { return m_seqNum; }

    /**
     * @brief 获取网络统计信息
     */
    struct Stats {
        quint32 totalSent     = 0;   // 总发送包数
        quint32 totalReceived = 0;   // 总接收包数
        quint32 lostPackets   = 0;   // 检测到的丢包数
        quint32 outOfOrder    = 0;   // 乱序包数
        quint32 invalidPackets = 0;  // 无效包数
    };
    Stats getStats() const { return m_stats; }

    /**
     * @brief 重置统计信息
     */
    void resetStats() { m_stats = Stats(); }

signals:
    /**
     * @brief 收到音频数据帧
     * @param opusPayload Opus 编码数据
     * @param seqNum      序列号
     * @param timestamp   时间戳
     */
    void audioFrameReceived(const QByteArray &opusPayload,
                            quint16 seqNum,
                            quint32 timestamp);

    /**
     * @brief 收到 PTT 信令
     * @param type      信令类型
     * @param timestamp 时间戳
     */
    void signalingReceived(FrameType type, quint32 timestamp);

    /**
     * @brief 检测到丢包
     * @param lostCount 连续丢失的包数
     */
    void packetLossDetected(int lostCount);

    /**
     * @brief 网络错误
     * @param error 错误信息
     */
    void networkError(const QString &error);

private slots:
    void onReadyRead();
    void onSendHeartbeat();

private:
    /**
     * @brief 发送一个完整的 UDP 包
     */
    void sendPacket(FrameType type, const QByteArray &payload);

    QUdpSocket     *m_socket     = nullptr;
    QHostAddress    m_peerAddr;
    quint16         m_peerPort   = 0;
    quint16         m_localPort  = 0;
    quint16         m_seqNum     = 0;         // 发送序列号
    qint32          m_lastRecvSeq = -1;       // 上次接收的序列号
    QTimer         *m_heartbeatTimer = nullptr;
    Stats           m_stats;

    static constexpr int HEARTBEAT_INTERVAL_MS = 1000;  // 心跳间隔 1 秒
    // 电台 MTU 通常较小，限制包大小
    static constexpr int MAX_UDP_PAYLOAD = 1200;
};

#endif // RADIOLINK_H
