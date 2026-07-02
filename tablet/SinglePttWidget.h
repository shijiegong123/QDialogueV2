/**
 * @file SinglePttWidget.h
 * @brief 1对1 PTT 对讲界面
 *
 * 功能：
 * - 顶部可设置车端 IP 和端口
 * - 连接/断开按钮
 * - PTT 按住说话大按钮
 * - 状态指示和音频电平
 * - 返回按钮回到模式选择
 */
#ifndef SINGLEPTTWIDGET_H
#define SINGLEPTTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QTimer>
#include "../common/MultiAudioManager.h"

class SinglePttWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SinglePttWidget(QWidget *parent = nullptr);
    ~SinglePttWidget();

    /**
     * @brief 设置默认连接参数
     */
    void setDefaultConfig(const QString &ip, quint16 localPort, quint16 peerPort);

signals:
    void backRequested();  // 用户点击返回

private slots:
    void onConnect();
    void onDisconnect();
    void onPttPressed();
    void onPttReleased();
    void onPttStateChanged(PTTState newState);
    void onAudioLevelChanged(float level);
    void onStatusTick();

private:
    void setupUI();
    void setupStyle();
    void updateStatusIndicator(PTTState state);
    void setConnected(bool connected);

    // ---- 连接配置区 ----
    QLineEdit    *m_ipEdit       = nullptr;
    QSpinBox     *m_localPortSpin = nullptr;
    QSpinBox     *m_peerPortSpin  = nullptr;
    QPushButton  *m_connectBtn    = nullptr;
    QPushButton  *m_disconnectBtn = nullptr;

    // ---- PTT 区域 ----
    QLabel       *m_statusLabel  = nullptr;
    QWidget      *m_statusLight  = nullptr;
    QPushButton  *m_pttButton    = nullptr;
    QProgressBar *m_levelBar     = nullptr;

    // ---- 底部信息 ----
    QLabel       *m_infoLabel    = nullptr;
    QPushButton  *m_backBtn      = nullptr;

    // ---- 音频管理器 ----
    MultiAudioManager *m_audioManager = nullptr;
    QTimer       *m_statusTimer  = nullptr;
    bool          m_connected    = false;
};

#endif // SINGLEPTTWIDGET_H
