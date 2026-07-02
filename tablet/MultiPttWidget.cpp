/**
 * @file MultiPttWidget.cpp
 * @brief 1对多 PTT 对讲界面实现
 */
#include "MultiPttWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QRegExpValidator>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

MultiPttWidget::MultiPttWidget(QWidget *parent)
    : QWidget(parent)
{
    // INI 文件路径: 应用程序同级目录
    m_iniFilePath = QCoreApplication::applicationDirPath() + "/cars.ini";

    setupUI();
    setupStyle();

    // 初始化 MultiAudioManager
    m_multiAudio = new MultiAudioManager(m_localPort, this);
    if (!m_multiAudio->initialize()) {
        qCritical() << "[MultiPtt] MultiAudioManager 初始化失败";
    } else {
        m_multiAudio->start();
    }

    connect(m_multiAudio, &MultiAudioManager::pttStateChanged,
            this, &MultiPttWidget::onPttStateChanged);
    connect(m_multiAudio, &MultiAudioManager::audioLevelChanged,
            this, &MultiPttWidget::onAudioLevelChanged);
    connect(m_multiAudio, &MultiAudioManager::carStatusChanged,
            this, &MultiPttWidget::onCarStatusChanged);

    // 加载保存的车端数据
    loadCars();

    // 状态刷新定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MultiPttWidget::onStatusTick);
    m_statusTimer->start(1000);
}

MultiPttWidget::~MultiPttWidget()
{
    if (m_multiAudio) {
        m_multiAudio->stop();
    }
}

// ==================== UI 构建 ====================

void MultiPttWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 15, 20, 15);
    mainLayout->setSpacing(10);

    // ---- 顶部: 返回 + 标题 ----
    QHBoxLayout *topBar = new QHBoxLayout();
    m_backBtn = new QPushButton("← 返回", this);
    m_backBtn->setObjectName("backBtn");
    m_backBtn->setFixedSize(100, 40);
    connect(m_backBtn, &QPushButton::clicked, this, &MultiPttWidget::backRequested);
    topBar->addWidget(m_backBtn);

    QLabel *title = new QLabel("1 对 多 对讲模式", this);
    title->setObjectName("pageTitle");
    title->setAlignment(Qt::AlignCenter);
    topBar->addWidget(title, 1);
    topBar->addSpacing(100);
    mainLayout->addLayout(topBar);

    // ---- 中间: 左右分栏 ----
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);

    // ======== 左侧: 车端列表 ========
    QVBoxLayout *leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(8);

    QLabel *listTitle = new QLabel("车端列表", this);
    listTitle->setObjectName("sectionTitle");
    leftPanel->addWidget(listTitle);

    m_carList = new QListWidget(this);
    m_carList->setObjectName("carList");
    m_carList->setMinimumWidth(280);
    m_carList->setSpacing(4);
    connect(m_carList, &QListWidget::itemClicked,
            this, &MultiPttWidget::onCarItemClicked);
    leftPanel->addWidget(m_carList, 1);

    // 广播按钮 (选中 = 发送给所有车端)
    m_broadcastBtn = new QPushButton("▶ 广播所有车端", this);
    m_broadcastBtn->setObjectName("broadcastBtn");
    m_broadcastBtn->setCheckable(true);
    m_broadcastBtn->setChecked(true);
    m_broadcastBtn->setFixedHeight(40);
    connect(m_broadcastBtn, &QPushButton::clicked, this, &MultiPttWidget::onBroadcastAll);
    leftPanel->addWidget(m_broadcastBtn);

    // 管理按钮行
    QHBoxLayout *mgmtBtns = new QHBoxLayout();
    m_addBtn = new QPushButton("+ 添加", this);
    m_addBtn->setObjectName("mgmtBtn");
    connect(m_addBtn, &QPushButton::clicked, this, &MultiPttWidget::onAddCar);
    mgmtBtns->addWidget(m_addBtn);

    m_editBtn = new QPushButton("✎ 编辑", this);
    m_editBtn->setObjectName("mgmtBtn");
    connect(m_editBtn, &QPushButton::clicked, this, &MultiPttWidget::onEditCar);
    mgmtBtns->addWidget(m_editBtn);

    m_removeBtn = new QPushButton("✕ 删除", this);
    m_removeBtn->setObjectName("removeBtn");
    connect(m_removeBtn, &QPushButton::clicked, this, &MultiPttWidget::onRemoveCar);
    mgmtBtns->addWidget(m_removeBtn);

    leftPanel->addLayout(mgmtBtns);
    contentLayout->addLayout(leftPanel, 2);

    // ======== 右侧: PTT 区域 ========
    QVBoxLayout *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(12);

    // 当前目标显示
    m_targetLabel = new QLabel("当前目标: ▶ 广播所有车端", this);
    m_targetLabel->setObjectName("targetLabel");
    m_targetLabel->setAlignment(Qt::AlignCenter);
    rightPanel->addWidget(m_targetLabel);

    // 状态指示
    QHBoxLayout *statusRow = new QHBoxLayout();
    m_statusLight = new QWidget(this);
    m_statusLight->setObjectName("statusLight");
    m_statusLight->setFixedSize(18, 18);
    statusRow->addStretch();
    statusRow->addWidget(m_statusLight);
    m_statusLabel = new QLabel("空闲", this);
    m_statusLabel->setObjectName("statusLabel");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    rightPanel->addLayout(statusRow);

    rightPanel->addStretch(1);

    // PTT 大按钮
    m_pttButton = new QPushButton("按住说话", this);
    m_pttButton->setObjectName("pttButton");
    m_pttButton->setMinimumSize(160, 160);
    m_pttButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_pttButton, &QPushButton::pressed,  this, &MultiPttWidget::onPttPressed);
    connect(m_pttButton, &QPushButton::released, this, &MultiPttWidget::onPttReleased);
    rightPanel->addWidget(m_pttButton, 0, Qt::AlignCenter);

    // 音频电平
    m_levelBar = new QProgressBar(this);
    m_levelBar->setObjectName("levelBar");
    m_levelBar->setRange(0, 100);
    m_levelBar->setValue(0);
    m_levelBar->setTextVisible(false);
    m_levelBar->setFixedHeight(12);
    rightPanel->addWidget(m_levelBar, 0, Qt::AlignCenter);

    rightPanel->addStretch(1);
    contentLayout->addLayout(rightPanel, 3);

    mainLayout->addLayout(contentLayout, 1);

    // ---- 底部信息 ----
    m_infoLabel = new QLabel("1对多模式 - 请添加车端", this);
    m_infoLabel->setObjectName("infoLabel");
    mainLayout->addWidget(m_infoLabel);
}

void MultiPttWidget::setupStyle()
{
    setStyleSheet(
        "MultiPttWidget { background-color: #1a1a2e; }"
        "#pageTitle { font-size: 22px; font-weight: bold; color: #e0e0e0; }"
        "#sectionTitle { font-size: 16px; font-weight: bold; color: #b0b0c0; }"
        "QLabel { color: #b0b0b0; font-size: 14px; }"

        "#backBtn {"
        "  background-color: #2d2d44; color: #aaa; border: 1px solid #444;"
        "  border-radius: 8px; font-size: 14px;"
        "}"

        "#carList {"
        "  background-color: #0d0d1a; color: #d0d0d0;"
        "  border: 2px solid #333355; border-radius: 8px;"
        "  font-size: 14px; padding: 5px;"
        "}"
        "#carList::item {"
        "  padding: 8px; border-bottom: 1px solid #222244;"
        "}"
        "#carList::item:selected {"
        "  background-color: #1a3a5c; border-left: 3px solid #2196f3;"
        "}"

        "#broadcastBtn {"
        "  background-color: #1a3a2a; color: #4caf50;"
        "  border: 2px solid #2e7d32; border-radius: 8px;"
        "  font-size: 14px; font-weight: bold;"
        "}"
        "#broadcastBtn:checked {"
        "  background-color: #2e7d32; color: white;"
        "}"

        "#mgmtBtn {"
        "  background-color: #16213e; color: #aaa;"
        "  border: 1px solid #333; border-radius: 6px;"
        "  font-size: 13px; padding: 8px;"
        "}"
        "#mgmtBtn:pressed { background-color: #1a3a5c; }"

        "#removeBtn {"
        "  background-color: #3e1616; color: #e57373;"
        "  border: 1px solid #5a2020; border-radius: 6px;"
        "  font-size: 13px; padding: 8px;"
        "}"
        "#removeBtn:pressed { background-color: #5a2020; }"

        "#targetLabel { font-size: 16px; font-weight: bold; color: #64b5f6; }"

        "#statusLight { background-color: #555; border-radius: 9px; }"
        "#statusLabel { font-size: 14px; color: #b0b0b0; }"

        "#pttButton {"
        "  background-color: #16213e; color: #e0e0e0;"
        "  font-size: 24px; font-weight: bold;"
        "  border: 4px solid #0f3460; border-radius: 80px;"
        "}"
        "#pttButton:pressed {"
        "  background-color: #e94560; border-color: #ff6b6b; color: white;"
        "}"

        "#levelBar {"
        "  border: 2px solid #333355; border-radius: 6px; background-color: #0a0a1a;"
        "}"
        "#levelBar::chunk {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #00c853, stop:0.6 #ffeb3b, stop:1 #f44336);"
        "  border-radius: 4px;"
        "}"

        "#infoLabel { font-size: 13px; color: #666688; }"
    );
}

// ==================== 车端管理 ====================

void MultiPttWidget::loadCars()
{
    m_cars = CarEndpointStore::load(m_iniFilePath);

    // 将加载的车端添加到 MultiAudioManager
    for (const auto &car : m_cars) {
        m_multiAudio->addCar(car);
    }

    refreshCarList();
    qDebug() << "[MultiPtt] 加载了" << m_cars.size() << "个车端";
}

void MultiPttWidget::saveCars()
{
    CarEndpointStore::save(m_cars, m_iniFilePath);
}

void MultiPttWidget::refreshCarList()
{
    m_carList->clear();

    for (const auto &car : m_cars) {
        QString statusIcon = car.online ? "●" : "○";
        QString text = QString("%1  %2  (%3:%4)")
                       .arg(statusIcon, car.name, car.ip)
                       .arg(car.port);

        QListWidgetItem *item = new QListWidgetItem(text, m_carList);
        item->setData(Qt::UserRole, car.id);
        item->setToolTip(QString("%1\nIP: %2\n端口: %3\n状态: %4")
                         .arg(car.name, car.ip)
                         .arg(car.port)
                         .arg(car.online ? "在线" : "离线"));
    }

    m_infoLabel->setText(QString("1对多模式 - 共 %1 个车端  |  本地端口: %2")
                         .arg(m_cars.size()).arg(m_localPort));
}

void MultiPttWidget::onAddCar()
{
    QString name, ip;
    int port = 9001;

    if (showCarDialog("添加车端", name, ip, port)) {
        CarEndpoint car;
        car.id   = CarEndpointStore::generateId();
        car.name = name;
        car.ip   = ip;
        car.port = static_cast<quint16>(port);

        m_cars.append(car);
        m_multiAudio->addCar(car);
        saveCars();
        refreshCarList();
    }
}

void MultiPttWidget::onEditCar()
{
    QListWidgetItem *item = m_carList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择要编辑的车端");
        return;
    }

    QString carId = item->data(Qt::UserRole).toString();
    int idx = -1;
    for (int i = 0; i < m_cars.size(); ++i) {
        if (m_cars[i].id == carId) { idx = i; break; }
    }
    if (idx < 0) return;

    QString name = m_cars[idx].name;
    QString ip   = m_cars[idx].ip;
    int     port = m_cars[idx].port;

    if (showCarDialog("编辑车端", name, ip, port, carId)) {
        m_cars[idx].name = name;
        m_cars[idx].ip   = ip;
        m_cars[idx].port = static_cast<quint16>(port);

        m_multiAudio->updateCar(m_cars[idx]);
        saveCars();
        refreshCarList();
    }
}

void MultiPttWidget::onRemoveCar()
{
    QListWidgetItem *item = m_carList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择要删除的车端");
        return;
    }

    QString carId = item->data(Qt::UserRole).toString();
    QString carName;
    for (const auto &car : m_cars) {
        if (car.id == carId) { carName = car.name; break; }
    }

    if (QMessageBox::question(this, "确认删除",
                              QString("确定要删除车端 \"%1\" 吗？").arg(carName),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_multiAudio->removeCar(carId);
        m_cars.erase(std::remove_if(m_cars.begin(), m_cars.end(),
                     [&carId](const CarEndpoint &c) { return c.id == carId; }),
                     m_cars.end());

        // 如果删除的是当前选中的目标，重置为广播
        if (m_selectedCarId == carId) {
            m_selectedCarId.clear();
            m_broadcastBtn->setChecked(true);
            updateTargetLabel();
        }

        saveCars();
        refreshCarList();
    }
}

bool MultiPttWidget::showCarDialog(const QString &title,
                                    QString &name, QString &ip, int &port,
                                    const QString &excludeId)
{
    // ---- 创建对话框 ----
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setMinimumWidth(420);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 15);

    // ---- 表单 ----
    QFormLayout *form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    QLineEdit *nameEdit = new QLineEdit(name, &dlg);
    nameEdit->setPlaceholderText("例: 小车1号");
    nameEdit->setMaxLength(30);
    form->addRow("车端名称:", nameEdit);

    QLineEdit *ipEdit = new QLineEdit(ip, &dlg);
    ipEdit->setPlaceholderText("例: 192.168.1.100");
    // IPv4 格式正则
    QRegExp ipRx("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$");
    ipEdit->setValidator(new QRegExpValidator(ipRx, &dlg));
    form->addRow("车端 IP:", ipEdit);

    QSpinBox *portSpin = new QSpinBox(&dlg);
    portSpin->setRange(1024, 65535);
    portSpin->setValue(port);
    form->addRow("车端端口:", portSpin);

    mainLayout->addLayout(form);

    // ---- 验证提示标签 ----
    QLabel *hintLabel = new QLabel(" ", &dlg);
    hintLabel->setStyleSheet("color: #f44336; font-size: 13px;");
    hintLabel->setWordWrap(true);
    hintLabel->setMinimumHeight(20);
    mainLayout->addWidget(hintLabel);

    // ---- 按钮 ----
    QDialogButtonBox *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText("确定");
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    mainLayout->addWidget(btnBox);

    QPushButton *okBtn = btnBox->button(QDialogButtonBox::Ok);
    okBtn->setEnabled(false);  // 初始禁用，输入有效后才启用

    // ---- 样式 ----
    dlg.setStyleSheet(
        "QDialog { background-color: #1e1e3a; }"
        "QLabel { color: #c0c0d0; font-size: 14px; }"
        "QLineEdit, QSpinBox {"
        "  background-color: #0d0d1a; color: #e0e0e0;"
        "  border: 1px solid #444466; border-radius: 6px;"
        "  padding: 6px; font-size: 14px;"
        "}"
        "QPushButton {"
        "  background-color: #16213e; color: #c0c0d0;"
        "  border: 1px solid #444; border-radius: 6px;"
        "  padding: 8px 20px; font-size: 14px;"
        "}"
        "QPushButton:disabled { color: #555; }"
        "QPushButton[text=\"确定\"] { background-color: #00695c; color: white; }"
    );

    // ---- 实时验证函数 ----
    auto validate = [&]() -> QString {
        // 名称检查
        QString n = nameEdit->text().trimmed();
        if (n.isEmpty())
            return "请输入车端名称";

        // IP 格式检查
        QString i = ipEdit->text().trimmed();
        if (i.isEmpty())
            return "请输入车端 IP 地址";
        if (!ipRx.exactMatch(i))
            return "IP 地址格式不正确 (例: 192.168.1.100)";

        // 重复检测: 名称不能重复
        for (const auto &car : m_cars) {
            if (car.id == excludeId) continue;  // 编辑时排除自身
            if (car.name == n)
                return QString("名称 \"%1\" 已存在，请更换").arg(n);
        }

        // 重复检测: IP+端口 不能重复
        int p = portSpin->value();
        for (const auto &car : m_cars) {
            if (car.id == excludeId) continue;
            if (car.ip == i && car.port == static_cast<quint16>(p))
                return QString("IP:端口 %1:%2 已被其他车端使用").arg(i).arg(p);
        }

        return QString();  // 空 = 验证通过
    };

    // ---- 输入变化时触发验证 ----
    auto onFieldChanged = [&]() {
        QString err = validate();
        hintLabel->setText(err.isEmpty() ? " " : err);
        okBtn->setEnabled(err.isEmpty());
    };

    QObject::connect(nameEdit, &QLineEdit::textChanged, onFieldChanged);
    QObject::connect(ipEdit,   &QLineEdit::textChanged, onFieldChanged);
    QObject::connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), onFieldChanged);

    // 初始验证一次 (编辑模式时预填数据)
    onFieldChanged();

    // ---- OK / Cancel ----
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        name = nameEdit->text().trimmed();
        ip   = ipEdit->text().trimmed();
        port = portSpin->value();
        return true;
    }
    return false;
}

// ==================== 目标选择 ====================

void MultiPttWidget::onCarItemClicked(QListWidgetItem *item)
{
    QString carId = item->data(Qt::UserRole).toString();

    // 切换选中状态
    if (m_selectedCarId == carId) {
        // 取消选中
        m_selectedCarId.clear();
        m_broadcastBtn->setChecked(true);
    } else {
        m_selectedCarId = carId;
        m_broadcastBtn->setChecked(false);
    }

    updateTargetLabel();
}

void MultiPttWidget::onBroadcastAll()
{
    m_selectedCarId.clear();
    m_broadcastBtn->setChecked(true);
    m_carList->clearSelection();
    updateTargetLabel();
}

void MultiPttWidget::updateTargetLabel()
{
    if (m_selectedCarId.isEmpty()) {
        m_targetLabel->setText("当前目标: ▶ 广播所有车端");
    } else {
        for (const auto &car : m_cars) {
            if (car.id == m_selectedCarId) {
                m_targetLabel->setText(QString("当前目标: ◎ %1 (%2)")
                                       .arg(car.name, car.ip));
                break;
            }
        }
    }
}

// ==================== PTT ====================

void MultiPttWidget::onPttPressed()
{
    if (m_multiAudio) {
        m_multiAudio->requestTalk(m_selectedCarId);
    }
}

void MultiPttWidget::onPttReleased()
{
    if (m_multiAudio) {
        m_multiAudio->releaseTalk();
    }
}

void MultiPttWidget::onPttStateChanged(PTTState newState)
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
        m_pttButton->setText("接收中...\n(对方说话)");
        m_pttButton->setEnabled(false);
        break;
    }
}

void MultiPttWidget::onAudioLevelChanged(float level)
{
    m_levelBar->setValue(static_cast<int>(level * 100));
}

void MultiPttWidget::onCarStatusChanged(const QString &carId, bool online)
{
    // 更新车端在线状态
    for (auto &car : m_cars) {
        if (car.id == carId) {
            car.online = online;
            break;
        }
    }
    refreshCarList();
}

void MultiPttWidget::updateStatusIndicator(PTTState state)
{
    QString color, text;
    switch (state) {
    case PTTState::IDLE:          color = "#555";    text = "空闲"; break;
    case PTTState::REQUESTING:    color = "#ffeb3b"; text = "请求中"; break;
    case PTTState::TRANSMITTING:  color = "#f44336"; text = "发送中"; break;
    case PTTState::RECEIVING:     color = "#00c853"; text = "接收中"; break;
    }
    m_statusLight->setStyleSheet(QString("background-color: %1; border-radius: 9px;").arg(color));
    m_statusLabel->setText(text);
}

void MultiPttWidget::onStatusTick()
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    int onlineCount = 0;
    for (const auto &car : m_cars) {
        if (car.online) onlineCount++;
    }
    m_infoLabel->setText(QString("1对多模式 - %1/%2 在线  |  %3")
                         .arg(onlineCount).arg(m_cars.size()).arg(time));
}
