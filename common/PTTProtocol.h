/**
 * @file PTTProtocol.h
 * @brief PTT 对讲协议定义 (应用层包头 + 信令)
 *
 * 协议设计原则：
 * - 简单高效，适配电台弱网环境
 * - 包头固定 12 字节，方便粘包/半包处理
 * - 信令使用独立帧类型，与音频数据分离
 *
 * === 双向 PTT 信令交互流程 ===
 *
 *  平板 (Tablet)                          小车 (Car)
 *      |                                     |
 *      |--- PTT_REQUEST (我要说话) -------->|
 *      |                                     | (检查是否空闲)
 *      |<-- PTT_GRANT (同意, 你说话吧) ------|
 *      |                                     |
 *      |=== AUDIO_DATA (Opus音频帧) =======>|  (小车播放)
 *      |=== AUDIO_DATA ====================>|
 *      |                                     |
 *      |--- PTT_RELEASE (我说完了) -------->|
 *      |                                     |
 *      |                                     |
 *      |<-- PTT_REQUEST (小车要说话) --------|
 *      |  (平板收到后停止发送，切换为接收)     |
 *      |--- PTT_GRANT (同意) -------------->|
 *      |                                     |
 *      |<== AUDIO_DATA (小车音频) ===========|
 *      |                                     |
 *      |<-- PTT_RELEASE (小车说完了) --------|
 *      |                                     |
 *
 *  超时机制：如果 3 秒没收到 PTT_RELEASE，自动释放通道。
 */
#ifndef PTTPROTOCOL_H
#define PTTPROTOCOL_H

#include <QtGlobal>
#include <QDataStream>
#include <QIODevice>
#include <QByteArray>

// ==================== 协议常量 ====================

// 协议魔数，用于识别合法数据包
static constexpr quint32 PTT_MAGIC = 0x50545400;  // "PTT\0"

// 协议版本
static constexpr quint8 PTT_VERSION = 1;

// 帧类型
enum class FrameType : quint8 {
    AUDIO_DATA    = 0x01,   // Opus 编码的音频帧
    PTT_REQUEST   = 0x10,   // 请求说话 (抢占通道)
    PTT_GRANT     = 0x11,   // 同意请求
    PTT_DENY      = 0x12,   // 拒绝请求 (对方正在说话)
    PTT_RELEASE   = 0x13,   // 释放通道 (说话结束)
    HEARTBEAT     = 0x20,   // 心跳保活
};

// PTT 状态
enum class PTTState {
    IDLE,           // 空闲，可以发起通话
    REQUESTING,     // 已发送 REQUEST，等待 GRANT
    TRANSMITTING,   // 正在发送音频 (我方说话)
    RECEIVING,      // 正在接收音频 (对方说话)
};

// ==================== 包头结构 ====================

/**
 * @brief UDP 数据包头部 (固定 12 字节)
 *
 *  偏移  大小  字段
 *  ----  ----  ----
 *  0     4     magic      (协议魔数 0x50545400)
 *  4     1     version    (协议版本)
 *  5     1     frameType  (帧类型)
 *  6     2     seqNum     (序列号, 0-65535 循环)
 *  8     4     timestamp  (发送时间戳, ms)
 *  12    N     payload    (负载数据)
 *
 */
struct PttPacketHeader {
    quint32 magic;
    quint8  version;
    quint8  frameType;
    quint16 seqNum;
    quint32 timestamp;

    static constexpr int HEADER_SIZE = 12;

    /**
     * @brief 序列化包头为字节流 (大端序, 网络字节序)
     */
    QByteArray serialize() const {
        QByteArray data;
        data.reserve(HEADER_SIZE);
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << magic << version << frameType << seqNum << timestamp;
        return data;
    }

    /**
     * @brief 从字节流反序列化包头
     * @return 解析成功返回 true
     */
    bool deserialize(const QByteArray &data) {
        if (data.size() < HEADER_SIZE) {
            return false;
        }
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> magic >> version >> frameType >> seqNum >> timestamp;
        return (magic == PTT_MAGIC && version == PTT_VERSION);
    }
};

// ==================== 辅助函数 ====================

/**
 * @brief 将 FrameType 转为可读字符串
 */
inline QString frameTypeToString(FrameType type) {
    switch (type) {
    case FrameType::AUDIO_DATA:   return "AUDIO_DATA";
    case FrameType::PTT_REQUEST:  return "PTT_REQUEST";
    case FrameType::PTT_GRANT:    return "PTT_GRANT";
    case FrameType::PTT_DENY:     return "PTT_DENY";
    case FrameType::PTT_RELEASE:  return "PTT_RELEASE";
    case FrameType::HEARTBEAT:    return "HEARTBEAT";
    default:                       return "UNKNOWN";
    }
}

/**
 * @brief 将 PTTState 转为可读字符串
 */
inline QString pttStateToString(PTTState state) {
    switch (state) {
    case PTTState::IDLE:          return "IDLE";
    case PTTState::REQUESTING:    return "REQUESTING";
    case PTTState::TRANSMITTING:  return "TRANSMITTING";
    case PTTState::RECEIVING:     return "RECEIVING";
    default:                       return "UNKNOWN";
    }
}

#endif // PTTPROTOCOL_H
