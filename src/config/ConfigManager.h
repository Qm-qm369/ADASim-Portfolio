#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>

// ADASim运行配置
struct AppConfig // 存参数。
{
    // 仿真
    double targetSpeed = 5.0;
    double planningDistance = 20.0;

    // 横向控制
    QString controllerMode = "DualError";
    double lookAheadDistance = 2.0;
    double headingGain = 1.0;
    double lateralGain = 1.5;

    // 网络
    int plannerPort = 8080;
};

class ConfigManager // 从硬盘读取/写入参数。
{
public:
    // 从INI文件读取配置
    static bool load(
        const QString &filePath,
        AppConfig &config,
        QString &errorMessage);

    // 把当前配置保存到INI文件
    static bool save(
        const QString &filePath,
        const AppConfig &config,
        QString &errorMessage);
};

#endif