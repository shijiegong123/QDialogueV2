/**
 * @file ModeSelectWidget.h
 * @brief 模式选择界面 - 选择 1对1 或 1对多
 */
#ifndef MODESELECTWIDGET_H
#define MODESELECTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class ModeSelectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModeSelectWidget(QWidget *parent = nullptr);

signals:
    void singleModeSelected();   // 选择了 1对1
    void multiModeSelected();    // 选择了 1对多

private:
    void setupUI();
    void setupStyle();

    QPushButton *m_singleBtn = nullptr;
    QPushButton *m_multiBtn  = nullptr;
    QLabel      *m_titleLabel = nullptr;
};

#endif // MODESELECTWIDGET_H
