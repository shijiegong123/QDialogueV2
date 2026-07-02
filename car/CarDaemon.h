/**
 * @file CarDaemon.h
 * @brief 小车端 PTT 后台守护进程
 *
 * 功能：
 * - 无 UI，纯后台运行
 * - 监听 UDP 端口，接收平板端音频并播放
 * - 支持本地麦克风按钮触发说话 (GPIO 或模拟)
 * - 日志输出到文件和终端
 * - 信号处理 (SIGTERM/SIGINT 优雅退出)
 */
#ifndef CARDAEMON_H
#define CARDAEMON_H

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include "../common/MultiAudioManager.h"

class CarDaemon : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit CarDaemon(QObject *parent = nullptr);
    ~CarDaemon();

    /**
     * @brief 配置并启动守护进程
     * @param localPort   本地 UDP 端口
     * @param peerAddr    平板端 IP
     * @param peerPort    平板端 UDP 端口
     * @param logFile     日志文件路径 (空字符串则只输出到终端)
     * @return 启动成功返回 true
     */
    bool start(quint16 localPort,
               const QString &peerAddr,
               quint16 peerPort,
               const QString &logFile = QString());

    /**
     * @brief 停止守护进程
     */
    void stop();

    /**
     * @brief 模拟小车端按下麦克风按钮 (开始说话)
     * 实际项目中可对接 GPIO 中断
     */
    void simulateMicPress();

    /**
     * @brief 模拟小车端松开麦克风按钮 (停止说话)
     */
    void simulateMicRelease();

private slots:
    // PTT 状态变化
    void onPttStateChanged(PTTState newState);

    // 网络错误
    void onError(const QString &error);

    // 状态心跳日志
    void onHeartbeat();

private:
    // 初始化日志系统
    void setupLogging(const QString &logFile);

    MultiAudioManager *m_audioManager = nullptr;
    QTimer       *m_heartbeatTimer = nullptr;
    QFile        *m_logFile = nullptr;
    QTextStream  *m_logStream = nullptr;
    bool          m_running = false;
};

#endif // CARDAEMON_H
