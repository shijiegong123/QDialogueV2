/**
 * @file main.cpp
 * @brief 平板端 PTT 对讲程序入口 (支持 1对1 / 1对多 模式切换)
 *
 * 使用方式:
 *   ./tablet_ptt                    # 启动后在界面选择模式
 *   ./tablet_ptt <小车IP>           # 直接进入 1对1 模式
 *   ./tablet_ptt <小车IP> 9000 9001 # 指定端口进入 1对1 模式
 */
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QStackedWidget>
#include <QSettings>
#include "ModeSelectWidget.h"
#include "SinglePttWidget.h"
#include "MultiPttWidget.h"
#include "../common/MultiAudioManager.h"

// 默认配置
static constexpr quint16 DEFAULT_LOCAL_PORT = 9000;
static constexpr quint16 DEFAULT_PEER_PORT  = 9001;
static const QString     DEFAULT_CAR_IP     = "172.23.100.9";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("PTT 语音对讲 - 平板端");
    app.setOrganizationName("QDialogue");

    qInfo() << "=== PTT 语音对讲 - 平板端 ===";

    // ---- 解析命令行参数 ----
    bool hasCmdArgs = (argc >= 2);
    QString peerAddr;
    quint16 localPort = DEFAULT_LOCAL_PORT;
    quint16 peerPort  = DEFAULT_PEER_PORT;

    if (hasCmdArgs) {
        peerAddr  = argv[1];
        localPort = (argc >= 3) ? QString(argv[2]).toUShort() : DEFAULT_LOCAL_PORT;
        peerPort  = (argc >= 4) ? QString(argv[3]).toUShort() : DEFAULT_PEER_PORT;
        qInfo() << "命令行模式: 对端" << peerAddr << ":" << peerPort;
    }

    // ---- 创建 QStackedWidget 作为主窗口容器 ----
    QStackedWidget stack;
    stack.setWindowTitle("PTT 语音对讲");
    stack.setMinimumSize(800, 600);

    // 页面索引
    enum PageIndex {
        PAGE_MODE_SELECT = 0,
        PAGE_SINGLE_PTT  = 1,
        PAGE_MULTI_PTT   = 2,
    };

    // ---- 创建各页面 ----
    ModeSelectWidget *modeSelect = new ModeSelectWidget();
    SinglePttWidget  *singlePtt  = new SinglePttWidget();
    MultiPttWidget   *multiPtt   = new MultiPttWidget();

    stack.addWidget(modeSelect);   // index 0
    stack.addWidget(singlePtt);    // index 1
    stack.addWidget(multiPtt);     // index 2

    // 设置 1对1 默认配置
    singlePtt->setDefaultConfig(
        hasCmdArgs ? peerAddr : DEFAULT_CAR_IP,
        localPort,
        peerPort
    );

    // ---- 连接页面切换信号 ----
    QObject::connect(modeSelect, &ModeSelectWidget::singleModeSelected, [&]() {
        stack.setCurrentIndex(PAGE_SINGLE_PTT);
        qInfo() << "切换到 1对1 模式";
    });

    QObject::connect(modeSelect, &ModeSelectWidget::multiModeSelected, [&]() {
        stack.setCurrentIndex(PAGE_MULTI_PTT);
        qInfo() << "切换到 1对多 模式";
    });

    // 返回按钮
    QObject::connect(singlePtt, &SinglePttWidget::backRequested, [&]() {
        stack.setCurrentIndex(PAGE_MODE_SELECT);
        qInfo() << "返回模式选择";
    });

    QObject::connect(multiPtt, &MultiPttWidget::backRequested, [&]() {
        stack.setCurrentIndex(PAGE_MODE_SELECT);
        qInfo() << "返回模式选择";
    });

    // ---- 如果有命令行参数，直接进入 1对1 模式 ----
    if (hasCmdArgs) {
        stack.setCurrentIndex(PAGE_SINGLE_PTT);
    } else {
        stack.setCurrentIndex(PAGE_MODE_SELECT);
    }

    // ---- 显示窗口 ----
    // 适配平板屏幕
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->availableGeometry();
        if (screenGeom.width() > 1024) {
            // 大屏幕平板
            stack.resize(1024, 768);
        } else {
            stack.resize(screenGeom.size());
            stack.showFullScreen();
        }
    } else {
        stack.resize(1024, 768);
    }

    stack.show();

    // ---- 运行事件循环 ----
    int ret = app.exec();

    return ret;
}
