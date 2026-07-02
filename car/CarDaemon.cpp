/**
 * @file CarDaemon.cpp
 * @brief 小车端 PTT 后台守护进程实现
 */
#include "CarDaemon.h"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <iostream>

CarDaemon::CarDaemon(QObject *parent)
    : QObject(parent)
{
}

CarDaemon::~CarDaemon()
{
    stop();
}

bool CarDaemon::start(quint16 localPort,
                       const QString &peerAddr,
                       quint16 peerPort,
                       const QString &logFile)
{
    if (m_running) return true;

    // 初始化日志
    setupLogging(logFile);

    qInfo() << "=== 小车端 PTT 守护进程启动 ===";
    qInfo() << "本地端口:" << localPort;
    qInfo() << "对端 (平板):" << peerAddr << ":" << peerPort;

    // ---- 创建 MultiAudioManager ----
    m_audioManager = new MultiAudioManager(localPort, this);

    // 可选：指定 ALSA 设备 (RK3588 平台)
    // m_audioManager->setInputDevice("sysdefault");
    // m_audioManager->setOutputDevice("sysdefault");

    if (!m_audioManager->initialize()) {
        qCritical() << "音频管理器初始化失败!";
        return false;
    }

    // 连接状态变化信号
    connect(m_audioManager, &MultiAudioManager::pttStateChanged,
            this, &CarDaemon::onPttStateChanged);
    connect(m_audioManager, &MultiAudioManager::errorOccurred,
            this, &CarDaemon::onError);

    // 启动音频管理器
    m_audioManager->start();

    // 添加对端 (平板) 为单车端 (小车端只有 1 对 1)
    CarEndpoint tablet;
    tablet.id   = "tablet";
    tablet.name = "平板";
    tablet.ip   = peerAddr;
    tablet.port = peerPort;
    m_audioManager->addCar(tablet);

    // ---- 心跳日志定时器 (每 30 秒输出一次状态) ----
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &CarDaemon::onHeartbeat);
    m_heartbeatTimer->start(30000);

    m_running = true;
    qInfo() << "守护进程已就绪，等待连接...";

    return true;
}

void CarDaemon::stop()
{
    if (!m_running) return;

    qInfo() << "正在停止守护进程...";

    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatTimer->deleteLater();
        m_heartbeatTimer = nullptr;
    }

    if (m_audioManager) {
        m_audioManager->stop();
        m_audioManager->deleteLater();
        m_audioManager = nullptr;
    }

    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }

    if (m_logFile) {
        m_logFile->close();
        m_logFile->deleteLater();
        m_logFile = nullptr;
    }

    m_running = false;
    qInfo() << "守护进程已停止";
}

void CarDaemon::simulateMicPress()
{
    qInfo() << "[CarDaemon] 模拟麦克风按钮按下";
    if (m_audioManager) {
        m_audioManager->requestTalk();
    }
}

void CarDaemon::simulateMicRelease()
{
    qInfo() << "[CarDaemon] 模拟麦克风按钮松开";
    if (m_audioManager) {
        m_audioManager->releaseTalk();
    }
}

void CarDaemon::onPttStateChanged(PTTState newState)
{
    qInfo() << "[CarDaemon] PTT 状态变化 ->" << pttStateToString(newState);
}

void CarDaemon::onError(const QString &error)
{
    qWarning() << "[CarDaemon] 错误:" << error;
}

void CarDaemon::onHeartbeat()
{
    qInfo() << "[CarDaemon] 心跳 - 状态:" << pttStateToString(m_audioManager->currentState())
            << "时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

void CarDaemon::setupLogging(const QString &logFile)
{
    if (logFile.isEmpty()) {
        return;  // 只使用 qDebug 输出到终端
    }

    m_logFile = new QFile(logFile, this);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream = new QTextStream(m_logFile);
        *m_logStream << "\n\n=== " << QDateTime::currentDateTime().toString()
                     << " 守护进程启动 ===\n";
        m_logStream->flush();
        qInfo() << "日志文件:" << logFile;
    } else {
        qWarning() << "无法打开日志文件:" << logFile;
    }
}
