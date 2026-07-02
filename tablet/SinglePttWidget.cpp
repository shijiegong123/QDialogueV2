/**
 * @file SinglePttWidget.cpp
 * @brief 1对1 PTT 对讲界面实现
 */
#include "SinglePttWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>

SinglePttWidget::SinglePttWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupStyle();

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &SinglePttWidget::onStatusTick);
    m_statusTimer->start(1000);
}

SinglePttWidget::~SinglePttWidget()
{
    onDisconnect();
}

void SinglePttWidget::setDefaultConfig(const QString &ip, quint16 localPort, quint16 peerPort)
{
    m_ipEdit->setText(ip);
    m_localPortSpin->setValue(localPort);
    m_peerPortSpin->setValue(peerPort);
}

void SinglePttWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 15, 20, 15);
    mainLayout->setSpacing(15);

    // ======== 顶部: 标题 + 返回 ========
    QHBoxLayout *topBar = new QHBoxLayout();
    m_backBtn = new QPushButton("← 返回", this);
    m_backBtn->setObjectName("backBtn");
    m_backBtn->setFixedSize(100, 40);
    connect(m_backBtn, &QPushButton::clicked, this, &SinglePttWidget::backRequested);
    topBar->addWidget(m_backBtn);

    QLabel *title = new QLabel("1 对 1 对讲模式", this);
    title->setObjectName("pageTitle");
    title->setAlignment(Qt::AlignCenter);
    topBar->addWidget(title, 1);
    topBar->addSpacing(100);  // 对称
    mainLayout->addLayout(topBar);

    // ======== 连接配置区 ========
    QGroupBox *configBox = new QGroupBox("车端连接配置", this);
    configBox->setObjectName("configBox");
    QGridLayout *configLayout = new QGridLayout(configBox);
    configLayout->setSpacing(10);
    configLayout->setContentsMargins(15, 20, 15, 15);

    configLayout->addWidget(new QLabel("车端 IP:", this), 0, 0);
    m_ipEdit = new QLineEdit(this);
    m_ipEdit->setObjectName("inputField");
    m_ipEdit->setPlaceholderText("例: 192.168.1.100");
    m_ipEdit->setText("172.23.100.9");
    m_ipEdit->setMinimumWidth(200);
    configLayout->addWidget(m_ipEdit, 0, 1);

    configLayout->addWidget(new QLabel("本地端口:", this), 0, 2);
    m_localPortSpin = new QSpinBox(this);
    m_localPortSpin->setObjectName("inputField");
    m_localPortSpin->setRange(1024, 65535);
    m_localPortSpin->setValue(9000);
    configLayout->addWidget(m_localPortSpin, 0, 3);

    configLayout->addWidget(new QLabel("车端端口:", this), 0, 4);
    m_peerPortSpin = new QSpinBox(this);
    m_peerPortSpin->setObjectName("inputField");
    m_peerPortSpin->setRange(1024, 65535);
    m_peerPortSpin->setValue(9001);
    configLayout->addWidget(m_peerPortSpin, 0, 5);

    // 连接/断开按钮
    QHBoxLayout *connBtnLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton("连接", this);
    m_connectBtn->setObjectName("connectBtn");
    m_connectBtn->setMinimumSize(100, 40);
    connect(m_connectBtn, &QPushButton::clicked, this, &SinglePttWidget::onConnect);
    connBtnLayout->addWidget(m_connectBtn);

    m_disconnectBtn = new QPushButton("断开", this);
    m_disconnectBtn->setObjectName("disconnectBtn");
    m_disconnectBtn->setMinimumSize(100, 40);
    m_disconnectBtn->setEnabled(false);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &SinglePttWidget::onDisconnect);
    connBtnLayout->addWidget(m_disconnectBtn);
    connBtnLayout->addStretch();

    configLayout->addLayout(connBtnLayout, 1, 0, 1, 6);
    mainLayout->addWidget(configBox);

    // ======== PTT 区域 ========
    QHBoxLayout *statusRow = new QHBoxLayout();
    m_statusLight = new QWidget(this);
    m_statusLight->setObjectName("statusLight");
    m_statusLight->setFixedSize(20, 20);
    statusRow->addWidget(m_statusLight);
    m_statusLabel = new QLabel("未连接", this);
    m_statusLabel->setObjectName("statusLabel");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    mainLayout->addLayout(statusRow);

    mainLayout->addStretch(1);

    // PTT 大按钮
    m_pttButton = new QPushButton("按住说话", this);
    m_pttButton->setObjectName("pttButton");
    m_pttButton->setMinimumSize(180, 180);
    m_pttButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pttButton->setEnabled(false);
    connect(m_pttButton, &QPushButton::pressed,  this, &SinglePttWidget::onPttPressed);
    connect(m_pttButton, &QPushButton::released, this, &SinglePttWidget::onPttReleased);
    mainLayout->addWidget(m_pttButton, 0, Qt::AlignCenter);

    // 音频电平条
    m_levelBar = new QProgressBar(this);
    m_levelBar->setObjectName("levelBar");
    m_levelBar->setRange(0, 100);
    m_levelBar->setValue(0);
    m_levelBar->setTextVisible(false);
    m_levelBar->setFixedHeight(14);
    m_levelBar->setMaximumWidth(400);
    mainLayout->addWidget(m_levelBar, 0, Qt::AlignCenter);

    mainLayout->addStretch(1);

    // ======== 底部信息 ========
    m_infoLabel = new QLabel("1对1模式 - 请先连接车端", this);
    m_infoLabel->setObjectName("infoLabel");
    mainLayout->addWidget(m_infoLabel);
}

void SinglePttWidget::setupStyle()
{
    setStyleSheet(
        "SinglePttWidget { background-color: #1a1a2e; }"

        "#pageTitle { font-size: 22px; font-weight: bold; color: #e0e0e0; }"

        "#backBtn {"
        "  background-color: #2d2d44; color: #aaa; border: 1px solid #444;"
        "  border-radius: 8px; font-size: 14px;"
        "}"
        "#backBtn:pressed { background-color: #3d3d55; }"

        "#configBox {"
        "  color: #b0b0b0; font-size: 14px;"
        "  border: 2px solid #333355; border-radius: 10px;"
        "  margin-top: 10px; padding-top: 15px;"
        "}"
        "#configBox::title { subcontrol-origin: margin; left: 15px; padding: 0 5px; }"

        "QLabel { color: #b0b0b0; font-size: 14px; }"

        "#inputField {"
        "  background-color: #0a0a1a; color: #e0e0e0;"
        "  border: 1px solid #333355; border-radius: 6px;"
        "  padding: 6px; font-size: 14px;"
        "}"

        "#connectBtn {"
        "  background-color: #00695c; color: white; border: none;"
        "  border-radius: 8px; font-size: 14px; font-weight: bold;"
        "}"
        "#connectBtn:pressed { background-color: #00897b; }"
        "#connectBtn:disabled { background-color: #333; color: #666; }"

        "#disconnectBtn {"
        "  background-color: #b71c1c; color: white; border: none;"
        "  border-radius: 8px; font-size: 14px; font-weight: bold;"
        "}"
        "#disconnectBtn:pressed { background-color: #d32f2f; }"
        "#disconnectBtn:disabled { background-color: #333; color: #666; }"

        "#statusLight { background-color: #555; border-radius: 10px; }"
        "#statusLabel { font-size: 16px; color: #b0b0b0; }"

        "#pttButton {"
        "  background-color: #16213e; color: #e0e0e0;"
        "  font-size: 26px; font-weight: bold;"
        "  border: 4px solid #0f3460; border-radius: 90px;"
        "}"
        "#pttButton:pressed {"
        "  background-color: #e94560; border-color: #ff6b6b; color: white;"
        "}"
        "#pttButton:disabled {"
        "  background-color: #111; color: #444; border-color: #222;"
        "}"

        "#levelBar {"
        "  border: 2px solid #333355; border-radius: 7px; background-color: #0a0a1a;"
        "}"
        "#levelBar::chunk {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #00c853, stop:0.6 #ffeb3b, stop:1 #f44336);"
        "  border-radius: 5px;"
        "}"

        "#infoLabel { font-size: 13px; color: #666688; }"
    );
}

// ==================== 连接管理 ====================

void SinglePttWidget::onConnect()
{
    QString ip = m_ipEdit->text().trimmed();
    quint16 localPort = m_localPortSpin->value();
    quint16 peerPort  = m_peerPortSpin->value();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入车端 IP 地址");
        return;
    }

    // 断开旧连接
    onDisconnect();

    // 创建新的 MultiAudioManager
    m_audioManager = new MultiAudioManager(localPort, this);

    if (!m_audioManager->initialize()) {
        QMessageBox::critical(this, "错误", "音频管理器初始化失败!\n请检查音频设备和 libopus。");
        delete m_audioManager;
        m_audioManager = nullptr;
        return;
    }

    connect(m_audioManager, &MultiAudioManager::pttStateChanged,
            this, &SinglePttWidget::onPttStateChanged);
    connect(m_audioManager, &MultiAudioManager::audioLevelChanged,
            this, &SinglePttWidget::onAudioLevelChanged);

    m_audioManager->start();

    // 添加单车端 (1对1 模式)
    CarEndpoint car;
    car.id   = "single";
    car.name = "小车";
    car.ip   = ip;
    car.port = peerPort;
    m_audioManager->addCar(car);
    setConnected(true);
}

void SinglePttWidget::onDisconnect()
{
    if (m_audioManager) {
        m_audioManager->stop();
        m_audioManager->deleteLater();
        m_audioManager = nullptr;
    }
    setConnected(false);
}

void SinglePttWidget::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_pttButton->setEnabled(connected);
    m_ipEdit->setEnabled(!connected);
    m_localPortSpin->setEnabled(!connected);
    m_peerPortSpin->setEnabled(!connected);

    if (connected) {
        m_statusLabel->setText("已连接 - 空闲");
        m_statusLight->setStyleSheet("background-color: #00c853; border-radius: 10px;");
        m_infoLabel->setText(QString("1对1模式 - 已连接到 %1:%2")
                             .arg(m_ipEdit->text()).arg(m_peerPortSpin->value()));
    } else {
        m_statusLabel->setText("未连接");
        m_statusLight->setStyleSheet("background-color: #555; border-radius: 10px;");
        m_infoLabel->setText("1对1模式 - 请先连接车端");
    }
}

// ==================== PTT 操作 ====================

void SinglePttWidget::onPttPressed()
{
    if (m_audioManager) m_audioManager->requestTalk();
}

void SinglePttWidget::onPttReleased()
{
    if (m_audioManager) m_audioManager->releaseTalk();
}

void SinglePttWidget::onPttStateChanged(PTTState newState)
{
    updateStatusIndicator(newState);
    switch (newState) {
    case PTTState::IDLE:
        m_pttButton->setText("按住说话");
        m_pttButton->setEnabled(true);
        m_levelBar->setValue(0);
        break;
    case PTTState::REQUESTING:
        m_pttButton->setText("等待中...");
        break;
    case PTTState::TRANSMITTING:
        m_pttButton->setText("正在说话\n松开结束");
        break;
    case PTTState::RECEIVING:
        m_pttButton->setText("接收中...");
        m_pttButton->setEnabled(false);
        break;
    }
}

void SinglePttWidget::onAudioLevelChanged(float level)
{
    m_levelBar->setValue(static_cast<int>(level * 100));
}

void SinglePttWidget::updateStatusIndicator(PTTState state)
{
    QString color, text;
    switch (state) {
    case PTTState::IDLE:          color = "#00c853"; text = "已连接 - 空闲"; break;
    case PTTState::REQUESTING:    color = "#ffeb3b"; text = "请求中..."; break;
    case PTTState::TRANSMITTING:  color = "#f44336"; text = "发送中"; break;
    case PTTState::RECEIVING:     color = "#2196f3"; text = "接收中"; break;
    }
    m_statusLight->setStyleSheet(QString("background-color: %1; border-radius: 10px;").arg(color));
    m_statusLabel->setText(text);
}

void SinglePttWidget::onStatusTick()
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    if (m_connected) {
        m_infoLabel->setText(QString("1对1模式 - 已连接到 %1:%2  |  %3")
                             .arg(m_ipEdit->text()).arg(m_peerPortSpin->value()).arg(time));
    }
}
