/**
 * @file RadioLink.cpp
 * @brief UDP 网络传输层实现
 */
#include "RadioLink.h"
#include <QDateTime>
#include <QDebug>

RadioLink::RadioLink(quint16 localPort,
                     const QString &peerAddr,
                     quint16 peerPort,
                     QObject *parent)
    : QObject(parent)
    , m_peerAddr(QHostAddress(peerAddr))
    , m_peerPort(peerPort)
    , m_localPort(localPort)
{
}

RadioLink::~RadioLink()
{
    stop();
}

bool RadioLink::start()
{
    if (m_socket) {
        return true;  // 已启动
    }

    m_socket = new QUdpSocket(this);

    // 设置 Socket 选项
    // 增大接收缓冲区，应对突发数据
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 256 * 1024);
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 64 * 1024);

    // 绑定本地端口，使用 ShareAddress 允许多个进程共用端口（调试用）
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_localPort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCritical() << "[RadioLink] 绑定端口" << m_localPort
                     << "失败:" << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &RadioLink::onReadyRead);

    // 启动心跳定时器
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &RadioLink::onSendHeartbeat);
    m_heartbeatTimer->start(HEARTBEAT_INTERVAL_MS);

    m_seqNum = 0;
    m_lastRecvSeq = -1;
    m_stats = Stats();

    qDebug() << "[RadioLink] 启动成功 - 本地端口:" << m_localPort
             << "对端:" << m_peerAddr.toString() << ":" << m_peerPort;

    return true;
}

void RadioLink::stop()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatTimer->deleteLater();
        m_heartbeatTimer = nullptr;
    }

    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    qDebug() << "[RadioLink] 已停止";
}

void RadioLink::sendAudioFrame(const QByteArray &opusPayload)
{
    sendPacket(FrameType::AUDIO_DATA, opusPayload);
}

void RadioLink::sendSignaling(FrameType type)
{
    sendPacket(type, QByteArray());
    qDebug() << "[RadioLink] 发送信令:" << frameTypeToString(type);
}

void RadioLink::sendPacket(FrameType type, const QByteArray &payload)
{
    if (!m_socket) {
        qWarning() << "[RadioLink] Socket 未初始化";
        return;
    }

    // 构建包头
    PttPacketHeader header;
    header.magic     = PTT_MAGIC;
    header.version   = PTT_VERSION;
    header.frameType = static_cast<quint8>(type);
    header.seqNum    = m_seqNum++;
    header.timestamp = static_cast<quint32>(
        QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);

    // 组装完整数据包
    QByteArray packet = header.serialize();
    packet.append(payload);

    // 检查包大小不超过 MTU 限制
    if (packet.size() > MAX_UDP_PAYLOAD) {
        qWarning() << "[RadioLink] 数据包过大:" << packet.size()
                    << "字节, 超过 MTU 限制" << MAX_UDP_PAYLOAD;
        return;
    }

    // 发送
    qint64 written = m_socket->writeDatagram(packet, m_peerAddr, m_peerPort);
    if (written != packet.size()) {
        qWarning() << "[RadioLink] 发送失败, 期望发送" << packet.size()
                    << "字节, 实际发送" << written << "字节"
                    << "错误:" << m_socket->errorString();
        emit networkError(m_socket->errorString());
    } else {
        m_stats.totalSent++;
    }
}

void RadioLink::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;
        m_socket->readDatagram(datagram.data(), datagram.size(),
                               &sender, &senderPort);

        // 解析包头
        PttPacketHeader header;
        if (!header.deserialize(datagram)) {
            m_stats.invalidPackets++;
            continue;
        }

        // 提取 payload
        QByteArray payload = datagram.mid(PttPacketHeader::HEADER_SIZE);

        m_stats.totalReceived++;

        // 序列号检测 (丢包/乱序)
        FrameType frameType = static_cast<FrameType>(header.frameType);

        if (frameType == FrameType::AUDIO_DATA) {
            // 只检测音频帧的序列号
            qint32 expectedSeq = (m_lastRecvSeq >= 0)
                ? (m_lastRecvSeq + 1) % 65536
                : header.seqNum;

            if (header.seqNum != expectedSeq) {
                int gap = (header.seqNum - m_lastRecvSeq + 65536) % 65536;
                if (gap > 1 && gap < 32768) {
                    // 正常丢包 (序列号递增但有间隔)
                    int lostCount = gap - 1;
                    m_stats.lostPackets += lostCount;
                    emit packetLossDetected(lostCount);
                } else {
                    // 乱序包 (序列号比上次小)
                    m_stats.outOfOrder++;
                    // 仍然处理，交给 JitterBuffer 排序
                }
            }
            m_lastRecvSeq = header.seqNum;

            emit audioFrameReceived(payload, header.seqNum, header.timestamp);
        } else if (frameType == FrameType::PTT_REQUEST ||
                   frameType == FrameType::PTT_GRANT ||
                   frameType == FrameType::PTT_DENY ||
                   frameType == FrameType::PTT_RELEASE) {
            // 信令帧，直接上报
            emit signalingReceived(frameType, header.timestamp);
        }
        // HEARTBEAT 帧忽略 (仅用于保活)
    }
}

void RadioLink::onSendHeartbeat()
{
    sendPacket(FrameType::HEARTBEAT, QByteArray());
}
