/**
 * @file CarEndpoint.h
 * @brief 车端数据端点定义
 *
 * 用于描述一个车端连接的信息：名称、IP、端口、在线状态。
 * 同时提供 QSettings 持久化读写方法。
 */
#ifndef CARENDPOINT_H
#define CARENDPOINT_H

#include <QString>
#include <QList>
#include <QSettings>
#include <QDateTime>

// ==================== 车端端点结构 ====================

struct CarEndpoint {
    QString id;          // 唯一标识 (UUID 或自定义)
    QString name;        // 显示名称 (如 "小车1号")
    QString ip;          // IP 地址
    quint16 port = 9001; // 对端 UDP 端口
    bool online  = false;   // 当前是否在线 (由心跳判断)

    // 构造
    CarEndpoint() = default;
    CarEndpoint(const QString &_id, const QString &_name,
                const QString &_ip, quint16 _port)
        : id(_id), name(_name), ip(_ip), port(_port) {}
};

// ==================== QSettings 持久化工具 ====================

class CarEndpointStore
{
public:
    /**
     * @brief 保存车端列表到 INI 文件
     * @param endpoints 车端列表
     * @param filePath  INI 文件路径 (默认当前目录下的 cars.ini)
     */
    static void save(const QList<CarEndpoint> &endpoints,
                     const QString &filePath = "cars.ini")
    {
        QSettings settings(filePath, QSettings::IniFormat);
        settings.beginWriteArray("cars");

        for (int i = 0; i < endpoints.size(); ++i) {
            const auto &ep = endpoints[i];
            settings.setArrayIndex(i);
            settings.setValue("id",   ep.id);
            settings.setValue("name", ep.name);
            settings.setValue("ip",   ep.ip);
            settings.setValue("port", ep.port);
        }

        settings.endArray();
        settings.sync();
    }

    /**
     * @brief 从 INI 文件加载车端列表
     * @param filePath INI 文件路径
     * @return 车端列表
     */
    static QList<CarEndpoint> load(const QString &filePath = "cars.ini")
    {
        QList<CarEndpoint> result;
        QSettings settings(filePath, QSettings::IniFormat);
        int count = settings.beginReadArray("cars");

        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            CarEndpoint ep;
            ep.id   = settings.value("id").toString();
            ep.name = settings.value("name").toString();
            ep.ip   = settings.value("ip").toString();
            ep.port = settings.value("port", 9001).toUInt();
            result.append(ep);
        }

        settings.endArray();
        return result;
    }

    /**
     * @brief 生成简单 UUID
     */
    static QString generateId()
    {
        return QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz")
               + QString::number(qrand() % 10000);
    }
};

#endif // CARENDPOINT_H
