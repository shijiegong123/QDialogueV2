/**
 * @file main.cpp
 * @brief 小车端 PTT 守护进程入口
 *
 * 启动逻辑:
 * 1. 优先读取同级目录下的 config.txt 配置文件。
 * 2. 如果 config.txt 存在，则严格校验参数，校验失败则退出。
 * 3. 如果 config.txt 不存在，则回退到命令行参数模式。
 */

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QHostAddress>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <signal.h>
#include "CarDaemon.h"

// 全局守护进程指针 (用于信号处理)
static CarDaemon *g_daemon = nullptr;

// Unix 信号处理函数
static void signalHandler(int signal)
{
    qInfo() << "收到信号" << signal << "，正在退出...";
    if (g_daemon) {
        g_daemon->stop();
    }
    QCoreApplication::quit();
}

// 默认端口配置
static constexpr quint16 DEFAULT_LOCAL_PORT = 9001;
static constexpr quint16 DEFAULT_PEER_PORT  = 9000;

// 配置结构体
struct CarConfig {
    QString peerIp;
    quint16 localPort = DEFAULT_LOCAL_PORT;
    quint16 peerPort  = DEFAULT_PEER_PORT;
    QString logFile;
};

/**
 * @brief 解析并校验配置文件
 * @param filePath 配置文件路径
 * @param config   输出配置结构体
 * @return 1: 成功读取且校验通过; 0: 文件不存在; -1: 读取或校验失败
 */
int loadAndValidateConfig(const QString &filePath, CarConfig &config) {
    QFile file(filePath);
    if (!file.exists()) {
        return 0; // 文件不存在
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "[Config] 错误: 无法打开配置文件:" << filePath;
        return -1;
    }

    qInfo() << "[Config] 正在读取配置文件:" << filePath;
    QTextStream in(&file);
    QMap<QString, QString> kvMap;

    // 1. 解析键值对
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        int eqIdx = line.indexOf('=');
        if (eqIdx > 0) {
            QString key = line.left(eqIdx).trimmed().toLower();
            QString val = line.mid(eqIdx + 1).trimmed();
            kvMap[key] = val;
        }
    }
    file.close();

    // 2. 校验必填参数: peer_ip
    if (!kvMap.contains("peer_ip") || kvMap["peer_ip"].isEmpty()) {
        qCritical() << "[Config] 校验失败: 缺少必填参数 'peer_ip'";
        return -1;
    }
    QHostAddress ipAddr(kvMap["peer_ip"]);
    if (ipAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        qCritical() << "[Config] 校验失败: 'peer_ip' 不是合法的 IPv4 地址:" << kvMap["peer_ip"];
        return -1;
    }
    config.peerIp = kvMap["peer_ip"];

    // 3. 校验必填参数: local_port
    if (!kvMap.contains("local_port") || kvMap["local_port"].isEmpty()) {
        qCritical() << "[Config] 校验失败: 缺少必填参数 'local_port'";
        return -1;
    }
    bool ok1 = false;
    quint16 lPort = kvMap["local_port"].toUShort(&ok1);
    if (!ok1 || lPort < 1024 || lPort > 65535) {
        qCritical() << "[Config] 校验失败: 'local_port' 必须是 1024-65535 之间的数字:" << kvMap["local_port"];
        return -1;
    }
    config.localPort = lPort;

    // 4. 校验必填参数: peer_port
    if (!kvMap.contains("peer_port") || kvMap["peer_port"].isEmpty()) {
        qCritical() << "[Config] 校验失败: 缺少必填参数 'peer_port'";
        return -1;
    }
    bool ok2 = false;
    quint16 pPort = kvMap["peer_port"].toUShort(&ok2);
    if (!ok2 || pPort < 1024 || pPort > 65535) {
        qCritical() << "[Config] 校验失败: 'peer_port' 必须是 1024-65535 之间的数字:" << kvMap["peer_port"];
        return -1;
    }
    config.peerPort = pPort;

    // 5. 校验选填参数: log_file
    if (kvMap.contains("log_file") && !kvMap["log_file"].isEmpty()) {
        QFileInfo logInfo(kvMap["log_file"]);
        QDir logDir = logInfo.absoluteDir();
        if (!logDir.exists()) {
            qWarning() << "[Config] 警告: 日志文件目录不存在，将尝试自动创建:" << logDir.absolutePath();
            if (!logDir.mkpath(".")) {
                qCritical() << "[Config] 校验失败: 无法创建日志目录:" << logDir.absolutePath();
                return -1;
            }
        }
        config.logFile = kvMap["log_file"];
    }

    qInfo() << "[Config] 配置文件读取并校验成功!";
    return 1;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("PTT 语音对讲 - 小车端");
    app.setOrganizationName("QDialogue");

    // ---- 注册信号处理 ----
    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    CarConfig config;
    bool configLoaded = false;

    // ---- 1. 尝试读取配置文件 ----
    QString configPath = QCoreApplication::applicationDirPath() + "/config.txt";
    int configStatus = loadAndValidateConfig(configPath, config);

    if (configStatus == 1) {
        // 配置文件读取且校验成功
        configLoaded = true;
    } else if (configStatus == -1) {
        // 配置文件存在，但解析或校验失败，直接退出
        qCritical() << "程序启动中止，请修正 config.txt 后重试。";
        return 1;
    }

    // ---- 2. 如果配置文件不存在，回退到命令行参数 ----
    if (!configLoaded) {
        qInfo() << "[Config] 未找到 config.txt，尝试使用命令行参数...";
        if (argc < 2) {
            qInfo() << "用法 1 (推荐): 在同级目录放置 config.txt 配置文件";
            qInfo() << "用法 2 (命令行): car_ptt <平板IP> [本地端口] [对端端口] [日志文件]";
            qInfo() << "示例: car_ptt 192.168.1.200 9001 9000 /var/log/car_ptt.log";
            return 1;
        }

        config.peerIp   = argv[1];
        config.localPort = (argc >= 3) ? QString(argv[2]).toUShort() : DEFAULT_LOCAL_PORT;
        config.peerPort  = (argc >= 4) ? QString(argv[3]).toUShort() : DEFAULT_PEER_PORT;
        config.logFile   = (argc >= 5) ? QString(argv[4]) : QString();

        // 简单校验命令行传入的 IP
        QHostAddress ipAddr(config.peerIp);
        if (ipAddr.protocol() != QAbstractSocket::IPv4Protocol) {
            qCritical() << "命令行参数错误: IP 地址不合法:" << config.peerIp;
            return 1;
        }
    }

    // ---- 3. 打印最终配置并启动 ----
    qInfo() << "=== PTT 语音对讲 - 小车端守护进程 ===";
    qInfo() << "对端 (平板):" << config.peerIp;
    qInfo() << "本地端口:" << config.localPort << " 对端端口:" << config.peerPort;
    if (!config.logFile.isEmpty()) {
        qInfo() << "日志文件:" << config.logFile;
    }

    // ---- 创建并启动守护进程 ----
    CarDaemon daemon;
    g_daemon = &daemon;
    if (!daemon.start(config.localPort, config.peerIp, config.peerPort, config.logFile)) {
        qCritical() << "守护进程启动失败!";
        return 2;
    }

    // ---- 运行事件循环 ----
    int ret = app.exec();

    // ---- 清理 ----
    g_daemon = nullptr;
    return ret;
}
