/**
 * @file MultiAudioManager.cpp
 * @brief 统一音频管理器实现 (合并原 AudioManager + MultiAudioManager)
 */
#include "MultiAudioManager.h"
#include <QDebug>
#include <QDateTime>
#include <cmath>

MultiAudioManager::MultiAudioManager(quint16 localPort, QObject *parent)
    : QObject(parent), m_localPort(localPort)
{
}

MultiAudioManager::~MultiAudioManager()
{
    stop();
}

bool MultiAudioManager::initialize()
{
    if (m_initialized) return true;

    qDebug() << "[AudioManager] 正在初始化...";

    // ---- 音频格式 ----
    m_audioFormat.setSampleRate(OpusCodec::SAMPLE_RATE);
    m_audioFormat.setChannelCount(OpusCodec::CHANNELS);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    // ---- Opus 编解码器 ----
    m_codec = new OpusCodec(24000, this);
    if (!m_codec->initialize()) {
        qCritical() << "[AudioManager] Opus 初始化失败";
        return false;
    }

    // ---- 音频输入/输出 ----
    if (!setupAudioInput()) {
        qCritical() << "[AudioManager] 音频输入初始化失败";
        return false;
    }
    if (!setupAudioOutput()) {
        qCritical() << "[AudioManager] 音频输出初始化失败";
        return false;
    }

    // ---- PTT 定时器 ----
    m_pttRequestTimer = new QTimer(this);
    m_pttRequestTimer->setSingleShot(true);
    connect(m_pttRequestTimer, &QTimer::timeout, this, &MultiAudioManager::onPttRequestTimeout);

    m_channelIdleTimer = new QTimer(this);
    m_channelIdleTimer->setSingleShot(true);
    connect(m_channelIdleTimer, &QTimer::timeout, this, &MultiAudioManager::onChannelIdleTimeout);

    // ---- 在线检测 (每2秒) ----
    m_onlineCheckTimer = new QTimer(this);
    connect(m_onlineCheckTimer, &QTimer::timeout, this, &MultiAudioManager::onCheckCarOnline);
    m_onlineCheckTimer->start(3000);

    m_initialized = true;
    qDebug() << "[AudioManager] 初始化成功";
    qDebug() << "[AudioManager] 可用输入设备:" << listInputDevices();
    qDebug() << "[AudioManager] 可用输出设备:" << listOutputDevices();
    return true;
}

void MultiAudioManager::start()
{
    if (!m_initialized) {
        qWarning() << "[AudioManager] 请先调用 initialize()";
        return;
    }
    startPlayback();
    setPttState(PTTState::IDLE);
    qDebug() << "[AudioManager] 已启动";
}

void MultiAudioManager::stop()
{
    stopCapture();
    stopPlayback();
    for (auto &link : m_carLinks) {
        if (link.jitter) link.jitter->stop();
        if (link.radio)  link.radio->stop();
    }
    setPttState(PTTState::IDLE);
    qDebug() << "[AudioManager] 已停止";
}

// ==================== 车端管理 ====================

void MultiAudioManager::addCar(const CarEndpoint &car)
{
    if (m_carLinks.contains(car.id)) {
        qWarning() << "[AudioManager] 车端已存在:" << car.id;
        return;
    }

    RadioLink *radio = new RadioLink(m_localPort, car.ip, car.port, this);
    if (!radio->start()) {
        qCritical() << "[AudioManager] RadioLink 启动失败:" << car.name;
        delete radio;
        return;
    }

    JitterBuffer *jitter = new JitterBuffer(m_codec, this);
    connect(jitter, &JitterBuffer::frameReady, this, &MultiAudioManager::onFrameReady);

    connect(radio, &RadioLink::audioFrameReceived, this, &MultiAudioManager::onAudioFrameReceived);
    connect(radio, &RadioLink::signalingReceived,   this, &MultiAudioManager::onSignalingReceived);
    connect(radio, &RadioLink::packetLossDetected,  this, &MultiAudioManager::onPacketLossDetected);
    connect(radio, &RadioLink::networkError,        this, &MultiAudioManager::errorOccurred);

    CarLink link;
    link.radio = radio;
    link.jitter = jitter;
    link.endpoint = car;
//    link.lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
    link.lastHeartbeatMs = 0;

    m_carLinks.insert(car.id, link);

    qDebug() << "[AudioManager] 添加车端:" << car.name << car.ip << ":" << car.port;
}

void MultiAudioManager::removeCar(const QString &carId)
{
    if (!m_carLinks.contains(carId)) return;
    auto link = m_carLinks.take(carId);
    if (link.jitter) { link.jitter->stop(); link.jitter->deleteLater(); }
    if (link.radio)  { link.radio->stop();  link.radio->deleteLater();  }
    qDebug() << "[AudioManager] 移除车端:" << link.endpoint.name;
}

void MultiAudioManager::updateCar(const CarEndpoint &car)
{
    removeCar(car.id);
    addCar(car);
}

QList<CarEndpoint> MultiAudioManager::cars() const
{
    QList<CarEndpoint> result;
    for (const auto &link : m_carLinks)
        result.append(link.endpoint);
    return result;
}

// ==================== PTT 操作 ====================

void MultiAudioManager::requestTalk(const QString &targetId)
{
    if (m_pttState == PTTState::TRANSMITTING) return;
    if (m_pttState == PTTState::RECEIVING) {
        qWarning() << "[AudioManager] 对方正在说话，无法抢占";
        return;
    }
    m_sendTargetId = targetId;
    sendSignalingToTargets(FrameType::PTT_REQUEST);
    setPttState(PTTState::REQUESTING);
    m_pttRequestTimer->start(PTT_REQUEST_TIMEOUT_MS);
}

void MultiAudioManager::releaseTalk()
{
    if (m_pttState != PTTState::TRANSMITTING) return;
    stopCapture();
    sendSignalingToTargets(FrameType::PTT_RELEASE);
    setPttState(PTTState::IDLE);
}

// ==================== 网络接收 ====================

void MultiAudioManager::onAudioFrameReceived(const QByteArray &opusPayload,
                                              quint16 seqNum, quint32 timestamp)
{
    Q_UNUSED(timestamp);
    if (m_pttState != PTTState::RECEIVING) {
        qDebug() << "[AudioManager] 收到音频帧但状态不是 RECEIVING:" << pttStateToString(m_pttState);
        return;
    }

    static int recvCount = 0;
    if (++recvCount % 50 == 0) {
        qDebug() << "[AudioManager] 收到音频帧 seq:" << seqNum
                 << "大小:" << opusPayload.size();
    }

    if (!m_currentSpeakerId.isEmpty() && m_carLinks.contains(m_currentSpeakerId)) {
        m_carLinks[m_currentSpeakerId].jitter->putFrame(opusPayload, seqNum);
        m_carLinks[m_currentSpeakerId].lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
    }

    m_channelIdleTimer->start(CHANNEL_IDLE_TIMEOUT_MS);
}

void MultiAudioManager::onSignalingReceived(FrameType type, quint32 timestamp)
{
    Q_UNUSED(timestamp);

    // 通过 QObject::sender() 判断是哪个 RadioLink
    RadioLink *senderRadio = qobject_cast<RadioLink *>(sender());
    QString carId;
    for (auto it = m_carLinks.constBegin(); it != m_carLinks.constEnd(); ++it) {
        if (it.value().radio == senderRadio) { carId = it.key(); break; }
    }
    if (carId.isEmpty()) return;

    m_carLinks[carId].lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();

    switch (type) {
    case FrameType::PTT_REQUEST:
        if (m_pttState == PTTState::IDLE) {
            m_carLinks[carId].radio->sendSignaling(FrameType::PTT_GRANT);
            m_currentSpeakerId = carId;
            setPttState(PTTState::RECEIVING);
            m_carLinks[carId].jitter->start();
            m_channelIdleTimer->start(CHANNEL_IDLE_TIMEOUT_MS);
            qDebug() << "[AudioManager] 同意车端说话:" << m_carLinks[carId].endpoint.name;
        } else if (m_pttState == PTTState::TRANSMITTING) {
            m_carLinks[carId].radio->sendSignaling(FrameType::PTT_DENY);
        } else if (m_pttState == PTTState::REQUESTING) {
            // 双方同时请求 -> 放弃自己转为接收
            m_carLinks[carId].radio->sendSignaling(FrameType::PTT_GRANT);
            stopCapture();
            m_currentSpeakerId = carId;
            setPttState(PTTState::RECEIVING);
            m_carLinks[carId].jitter->start();
            m_pttRequestTimer->stop();
            qDebug() << "[AudioManager] 冲突仲裁：放弃发送，转为接收";
        }
        emit signalingFromCar(carId, type);
        break;

    case FrameType::PTT_GRANT:
        if (m_pttState == PTTState::REQUESTING) {
            m_pttRequestTimer->stop();
            setPttState(PTTState::TRANSMITTING);
            startCapture();
            qDebug() << "[AudioManager] 获得说话权限，开始采集";
        }
        break;

    case FrameType::PTT_DENY:
        if (m_pttState == PTTState::REQUESTING) {
            m_pttRequestTimer->stop();
            setPttState(PTTState::IDLE);
            qDebug() << "[AudioManager] 说话请求被拒绝";
        }
        break;

    case FrameType::PTT_RELEASE:
        if (m_pttState == PTTState::RECEIVING && m_currentSpeakerId == carId) {
            m_carLinks[carId].jitter->stop();
            m_channelIdleTimer->stop();
            m_currentSpeakerId.clear();
            setPttState(PTTState::IDLE);
            qDebug() << "[AudioManager] 对方停止说话，通道释放";
        }
        emit signalingFromCar(carId, type);
        break;

    default:
        break;
    }
}

void MultiAudioManager::onPacketLossDetected(int lostCount)
{
    qDebug() << "[AudioManager] 检测到丢包:" << lostCount << "帧";
}

// ==================== JitterBuffer 输出 -> 播放 ====================

void MultiAudioManager::onFrameReady(const QByteArray &pcmData)
{
    if (m_outputDevice && m_audioOutput && m_pttState == PTTState::RECEIVING) {
        // 允许 ActiveState 和 IdleState 下写入（IdleState 写入后自动激活）
        QAudio::State state = m_audioOutput->state();

        if (state == QAudio::ActiveState || state == QAudio::IdleState) {
            qint64 written = m_outputDevice->write(pcmData);

            static int playCount = 0;
            if (++playCount % 50 == 0) {
                qDebug() << "[AudioManager] 写入播放设备, PCM大小:" << pcmData.size()
                         << "实际写入:" << written << "音频状态:" << state;
            }

            emit audioLevelChanged(calculateAudioLevel(pcmData));

        } else if (state == QAudio::StoppedState || state == QAudio::SuspendedState) {
            qWarning() << "[AudioManager] 音频输出状态异常:" << state << "尝试重启...";
            startPlayback();
        }
    }
}

// ==================== PTT 超时 ====================

void MultiAudioManager::onPttRequestTimeout()
{
    if (m_pttState == PTTState::REQUESTING) {
        qWarning() << "[AudioManager] PTT 请求超时，直接进入发送模式";
        setPttState(PTTState::TRANSMITTING);
        startCapture();
    }
}

void MultiAudioManager::onChannelIdleTimeout()
{
    if (m_pttState == PTTState::RECEIVING) {
        if (!m_currentSpeakerId.isEmpty() && m_carLinks.contains(m_currentSpeakerId))
            m_carLinks[m_currentSpeakerId].jitter->stop();
        m_currentSpeakerId.clear();
        setPttState(PTTState::IDLE);
        qWarning() << "[AudioManager] 通道空闲超时，自动释放";
    }
}

void MultiAudioManager::onCheckCarOnline()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_carLinks.begin(); it != m_carLinks.end(); ++it) {
        bool was = it.value().endpoint.online;
        bool is  = (now - it.value().lastHeartbeatMs) < HEARTBEAT_TIMEOUT_MS;

        if (was != is) {
            it.value().endpoint.online = is;
            emit carStatusChanged(it.key(), is);
            qDebug() << "[AudioManager] 车端" << it.value().endpoint.name
                     << (is ? "上线" : "离线");
        }
    }
}

void MultiAudioManager::onCaptureReady()
{
    if (!m_inputDevice) return;
    QByteArray data = m_inputDevice->readAll();
    m_captureBuffer.append(data);

    while (m_captureBuffer.size() >= OpusCodec::FRAME_PCM_SIZE) {
        QByteArray frame = m_captureBuffer.left(OpusCodec::FRAME_PCM_SIZE);
        m_captureBuffer.remove(0, OpusCodec::FRAME_PCM_SIZE);

        QByteArray encoded = m_codec->encode(frame);
        if (!encoded.isEmpty()) {
            static int sendCount = 0;
            if (++sendCount % 50 == 0)
                qDebug() << "[AudioManager] 发送音频帧, 大小:" << encoded.size();
            sendAudioToTargets(encoded);
        }
        emit audioLevelChanged(calculateAudioLevel(frame));
    }
}

// ==================== 发送辅助 ====================

void MultiAudioManager::sendAudioToTargets(const QByteArray &opusData)
{
    if (m_sendTargetId.isEmpty()) {
        for (auto &link : m_carLinks)
            link.radio->sendAudioFrame(opusData);
    } else if (m_carLinks.contains(m_sendTargetId)) {
        m_carLinks[m_sendTargetId].radio->sendAudioFrame(opusData);
    }
}

void MultiAudioManager::sendSignalingToTargets(FrameType type)
{
    if (m_sendTargetId.isEmpty()) {
        for (auto &link : m_carLinks)
            link.radio->sendSignaling(type);
    } else if (m_carLinks.contains(m_sendTargetId)) {
        m_carLinks[m_sendTargetId].radio->sendSignaling(type);
    }
}

void MultiAudioManager::setPttState(PTTState newState)
{
    if (m_pttState != newState) {
        qDebug() << "[AudioManager] PTT 状态:" << pttStateToString(m_pttState)
                 << "->" << pttStateToString(newState);
        m_pttState = newState;
        emit pttStateChanged(newState);
    }
}

// ==================== 音频设备 (合并自 AudioManager) ====================

bool MultiAudioManager::setupAudioInput()
{
    QAudioDeviceInfo info;
    if (!m_inputDeviceName.isEmpty()) {
        info = findInputDevice(m_inputDeviceName);
        if (info.isNull())
            qWarning() << "[AudioManager] 未找到输入设备:" << m_inputDeviceName << "使用默认";
    }
    if (info.isNull()) info = QAudioDeviceInfo::defaultInputDevice();
    if (info.isNull()) {
        qCritical() << "[AudioManager] 无可用输入设备! 可用:" << listInputDevices();
        return false;
    }
    if (!info.isFormatSupported(m_audioFormat)) {
        m_audioFormat = info.nearestFormat(m_audioFormat);
        qWarning() << "[AudioManager] 输入格式调整:" << m_audioFormat.sampleRate() << "Hz";
    }
    m_audioInput = new QAudioInput(info, m_audioFormat, this);
    m_audioInput->setBufferSize(OpusCodec::FRAME_PCM_SIZE * 4);
    qDebug() << "[AudioManager] 输入设备:" << info.deviceName();
    return true;
}

bool MultiAudioManager::setupAudioOutput()
{
    QAudioDeviceInfo info;
    if (!m_outputDeviceName.isEmpty()) {
        info = findOutputDevice(m_outputDeviceName);
        if (info.isNull())
            qWarning() << "[AudioManager] 未找到输出设备:" << m_outputDeviceName << "使用默认";
    }
    if (info.isNull()) info = QAudioDeviceInfo::defaultOutputDevice();
    if (info.isNull()) {
        qCritical() << "[AudioManager] 无可用输出设备! 可用:" << listOutputDevices();
        return false;
    }
    if (!info.isFormatSupported(m_audioFormat)) {
        m_audioFormat = info.nearestFormat(m_audioFormat);
        qWarning() << "[AudioManager] 输出格式调整:" << m_audioFormat.sampleRate() << "Hz";
    }
    m_audioOutput = new QAudioOutput(info, m_audioFormat, this);
    m_audioOutput->setBufferSize(OpusCodec::FRAME_PCM_SIZE * 4);
    qDebug() << "[AudioManager] 输出设备:" << info.deviceName();
    return true;
}

void MultiAudioManager::startCapture()
{
    if (!m_audioInput) return;
    m_captureBuffer.clear();
    m_captureBuffer.reserve(OpusCodec::FRAME_PCM_SIZE);
    m_inputDevice = m_audioInput->start();
    if (!m_inputDevice) {
        qCritical() << "[AudioManager] 启动采集失败:" << m_audioInput->error();
        return;
    }
    connect(m_inputDevice, &QIODevice::readyRead, this, &MultiAudioManager::onCaptureReady);
    qDebug() << "[AudioManager] 音频采集已启动";
}

void MultiAudioManager::stopCapture()
{
    if (m_audioInput) { m_audioInput->stop(); m_inputDevice = nullptr; }
    m_captureBuffer.clear();
}

void MultiAudioManager::startPlayback()
{
    if (!m_audioOutput) return;
    m_outputDevice = m_audioOutput->start();
    if (!m_outputDevice) {
        qCritical() << "[AudioManager] 启动输出失败:" << m_audioOutput->error();
        return;
    }
    qDebug() << "[AudioManager] 音频输出已启动";
}

void MultiAudioManager::stopPlayback()
{
    if (m_audioOutput) { m_audioOutput->stop(); m_outputDevice = nullptr; }
}

// ==================== ALSA 设备工具 ====================

void MultiAudioManager::setInputDevice(const QString &name)  { m_inputDeviceName = name; }
void MultiAudioManager::setOutputDevice(const QString &name) { m_outputDeviceName = name; }

QStringList MultiAudioManager::listInputDevices()
{
    QStringList r;
    for (const auto &d : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        r << d.deviceName();
    return r;
}

QStringList MultiAudioManager::listOutputDevices()
{
    QStringList r;
    for (const auto &d : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        r << d.deviceName();
    return r;
}

QAudioDeviceInfo MultiAudioManager::findInputDevice(const QString &name)
{
    for (const auto &d : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        if (d.deviceName().contains(name, Qt::CaseInsensitive)) return d;
    return {};
}

QAudioDeviceInfo MultiAudioManager::findOutputDevice(const QString &name)
{
    for (const auto &d : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        if (d.deviceName().contains(name, Qt::CaseInsensitive)) return d;
    return {};
}

float MultiAudioManager::calculateAudioLevel(const QByteArray &pcmData)
{
    if (pcmData.size() < 2) return 0.0f;
    const qint16 *s = reinterpret_cast<const qint16 *>(pcmData.constData());
    int n = pcmData.size() / 2;
    double sum = 0;
    for (int i = 0; i < n; ++i) { double v = s[i] / 32768.0; sum += v * v; }
    return static_cast<float>(qMin(1.0, std::sqrt(sum / n) * 3.0));
}
