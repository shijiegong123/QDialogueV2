/**
 * @file ModeSelectWidget.cpp
 * @brief 模式选择界面实现
 */
#include "ModeSelectWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

ModeSelectWidget::ModeSelectWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupStyle();
}

void ModeSelectWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(30);

    // 标题
    m_titleLabel = new QLabel("PTT 语音对讲系统", this);
    m_titleLabel->setObjectName("modeTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    layout->addSpacing(60);
    layout->addWidget(m_titleLabel);

    // 副标题
    QLabel *subLabel = new QLabel("请选择对讲模式", this);
    subLabel->setObjectName("modeSubTitle");
    subLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subLabel);

    layout->addSpacing(40);

    // 两个大按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(40);

    m_singleBtn = new QPushButton(this);
    m_singleBtn->setObjectName("modeBtn");
    m_singleBtn->setText("1 对 1\n\n单车对讲\n指定一台小车通信");
    m_singleBtn->setMinimumSize(280, 220);
    m_singleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btnLayout->addWidget(m_singleBtn);

    m_multiBtn = new QPushButton(this);
    m_multiBtn->setObjectName("modeBtn");
    m_multiBtn->setText("1 对 多\n\n多车对讲\n同时管理多台小车");
    m_multiBtn->setMinimumSize(280, 220);
    m_multiBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btnLayout->addWidget(m_multiBtn);

    layout->addLayout(btnLayout);
    layout->addSpacing(60);

    // 连接信号
    connect(m_singleBtn, &QPushButton::clicked, this, &ModeSelectWidget::singleModeSelected);
    connect(m_multiBtn,  &QPushButton::clicked, this, &ModeSelectWidget::multiModeSelected);
}

void ModeSelectWidget::setupStyle()
{
    setStyleSheet(
        "ModeSelectWidget { background-color: #1a1a2e; }"

        "#modeTitle {"
        "  font-size: 32px; font-weight: bold; color: #e0e0e0;"
        "}"

        "#modeSubTitle {"
        "  font-size: 20px; color: #8888aa;"
        "}"

        "#modeBtn {"
        "  background-color: #16213e;"
        "  color: #e0e0e0;"
        "  font-size: 22px;"
        "  font-weight: bold;"
        "  border: 3px solid #0f3460;"
        "  border-radius: 20px;"
        "  padding: 20px;"
        "}"
        "#modeBtn:hover {"
        "  background-color: #1a2a4e;"
        "  border-color: #2980b9;"
        "}"
        "#modeBtn:pressed {"
        "  background-color: #0f3460;"
        "}"
    );
}
