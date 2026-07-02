/**
 * @file MultiAudioManager.h
 * @brief 统一音频管理器 - 同时支持 1对1 和 1对多 PTT 对讲
 *
 * 设计原则：
 * - 1对1 = addCar() 添加一个车端
 * - 1对多 = addCar() 添加多个车端
 * - 每个车端对应一个 RadioLink + JitterBuffer
 * - 发送可选单播/广播，接收自动识别来源
 * - 包含 ALSA 设备选择、在线检测、音频电平
 */
#ifndef MULTIAUDIOMANAGER_H
#define MULTIAUDIOMANAGER_H

#include <QObject>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QTimer>
#include <QAudioDeviceInfo>
#include <QMap>
#include "OpusCodec.h"
#include "RadioLink.h"
#include "JitterBuffer.h"
#include "PTTProtocol.h"
#include "CarEndpoint.h"

class MultiAudioManager : public QObject
{
    Q_OBJECT

public:
    explicit MultiAudioManager(quint16 localPort, QObject *parent = nullptr);
    ~MultiAudioManager();

    bool initialize();
    void start();
    void stop();

    // ==================== 车端管理 ====================
    void addCar(const CarEndpoint &car);
    void removeCar(const QString &carId);
    void updateCar(const CarEndpoint &car);
    QList<CarEndpoint> cars() const;

    // ==================== PTT 操作 ====================
    /**
     * @brief 请求说话
     * @param targetId 目标车端 ID，空 = 广播所有
     */
    void requestTalk(const QString &targetId = QString());
    void releaseTalk();

    PTTState currentState() const { return m_pttState; }
    QString  currentSpeakerId() const { return m_currentSpeakerId; }

    // ==================== ALSA 设备 ====================
    void setInputDevice(const QString &deviceName);
    void setOutputDevice(const QString &deviceName);
    static QStringList listInputDevices();
    static QStringList listOutputDevices();

signals:
    void pttStateChanged(PTTState newState);
    void audioLevelChanged(float level);
    void carStatusChanged(const QString &carId, bool online);
    void errorOccurred(const QString &error);
    void signalingFromCar(const QString &carId, FrameType type);

private slots:
    void onAudioFrameReceived(const QByteArray &opusPayload,
                              quint16 seqNum, quint32 timestamp);
    void onSignalingReceived(FrameType type, quint32 timestamp);
    void onPacketLossDetected(int lostCount);
    void onFrameReady(const QByteArray &pcmData);
    void onPttRequestTimeout();
    void onChannelIdleTimeout();
    void onCheckCarOnline();
    void onCaptureReady();

private:
    // ---- 音频设备 ----
    bool setupAudioInput();
    bool setupAudioOutput();
    void startCapture();
    void stopCapture();
    void startPlayback();
    void stopPlayback();
    static QAudioDeviceInfo findInputDevice(const QString &name);
    static QAudioDeviceInfo findOutputDevice(const QString &name);

    // ---- PTT ----
    void setPttState(PTTState newState);

    // ---- 发送 ----
    void sendAudioToTargets(const QByteArray &opusData);
    void sendSignalingToTargets(FrameType type);

    // ---- 工具 ----
    float calculateAudioLevel(const QByteArray &pcmData);

    // ==================== 成员变量 ====================

    OpusCodec *m_codec = nullptr;

    // 每个车端一个 RadioLink + JitterBuffer
    struct CarLink {
        RadioLink    *radio    = nullptr;
        JitterBuffer *jitter   = nullptr;
        CarEndpoint   endpoint;
        qint64        lastHeartbeatMs = 0;
    };
    QMap<QString, CarLink> m_carLinks;
    QString m_sendTargetId;      // 空=广播

    // ---- 音频输入 ----
    QAudioInput  *m_audioInput  = nullptr;
    QIODevice    *m_inputDevice = nullptr;
    QByteArray    m_captureBuffer;
    QAudioFormat  m_audioFormat;
    QString       m_inputDeviceName;

    // ---- 音频输出 ----
    QAudioOutput *m_audioOutput  = nullptr;
    QIODevice    *m_outputDevice = nullptr;
    QString       m_outputDeviceName;

    // ---- PTT 状态 ----
    PTTState  m_pttState = PTTState::IDLE;
    QTimer   *m_pttRequestTimer  = nullptr;
    QTimer   *m_channelIdleTimer = nullptr;
    QString   m_currentSpeakerId;

    // ---- 在线检测 ----
    QTimer   *m_onlineCheckTimer = nullptr;

    // ---- 配置 ----
    quint16  m_localPort;
    bool     m_initialized = false;

    static constexpr int HEARTBEAT_TIMEOUT_MS     = 5000;
    static constexpr int PTT_REQUEST_TIMEOUT_MS   = 3000;
    static constexpr int CHANNEL_IDLE_TIMEOUT_MS  = 5000;
};

#endif // MULTIAUDIOMANAGER_H
