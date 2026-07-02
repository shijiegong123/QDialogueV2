/**
 * @file MultiPttWidget.h
 * @brief 1对多 PTT 对讲界面
 *
 * 功能：
 * - 左侧: 车端列表 (显示名称/IP/在线状态)
 * - 支持添加、编辑、删除车端
 * - 选择通信目标 (单车/全部广播)
 * - 右侧: PTT 按住说话按钮
 * - QSettings 持久化车端数据 (cars.ini)
 */
#ifndef MULTIPPTWIDGET_H
#define MULTIPPTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QTimer>
#include <QMenu>
#include "../common/MultiAudioManager.h"
#include "../common/CarEndpoint.h"

class MultiPttWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MultiPttWidget(QWidget *parent = nullptr);
    ~MultiPttWidget();

signals:
    void backRequested();

private slots:
    // 车端管理
    void onAddCar();
    void onEditCar();
    void onRemoveCar();

    // 选择通信目标
    void onCarItemClicked(QListWidgetItem *item);
    void onBroadcastAll();

    // PTT
    void onPttPressed();
    void onPttReleased();
    void onPttStateChanged(PTTState newState);
    void onAudioLevelChanged(float level);

    // 车端状态更新
    void onCarStatusChanged(const QString &carId, bool online);

    // 定时刷新
    void onStatusTick();

private:
    void setupUI();
    void setupStyle();
    void loadCars();
    void saveCars();
    void refreshCarList();
    void updateTargetLabel();
    void updateStatusIndicator(PTTState state);

    // 弹出添加/编辑车端对话框 (统一弹窗，含格式检测和重复检测)
    // excludeId: 编辑时排除自身，添加时为空
    bool showCarDialog(const QString &title,
                       QString &name, QString &ip, int &port,
                       const QString &excludeId = QString());

    // ---- UI 组件 ----
    // 左侧: 车端列表
    QListWidget  *m_carList      = nullptr;
    QPushButton  *m_addBtn       = nullptr;
    QPushButton  *m_editBtn      = nullptr;
    QPushButton  *m_removeBtn    = nullptr;
    QPushButton  *m_broadcastBtn = nullptr;

    // 右侧: PTT
    QLabel       *m_targetLabel  = nullptr;
    QWidget      *m_statusLight  = nullptr;
    QLabel       *m_statusLabel  = nullptr;
    QPushButton  *m_pttButton    = nullptr;
    QProgressBar *m_levelBar     = nullptr;

    // 底部
    QLabel       *m_infoLabel    = nullptr;
    QPushButton  *m_backBtn      = nullptr;

    // ---- 数据 ----
    QList<CarEndpoint> m_cars;
    QString  m_selectedCarId;   // 空=广播, 非空=指定车端
    QString  m_iniFilePath;

    // ---- 音频管理 ----
    MultiAudioManager *m_multiAudio = nullptr;
    QTimer     *m_statusTimer = nullptr;
    quint16     m_localPort = 9000;
};

#endif // MULTIPPTWIDGET_H
