#include "ConfigManager.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>

bool ConfigManager::load(
    const QString &filePath,
    AppConfig &config,
    QString &errorMessage)
{
    // QFileInfo 是 Qt 自带的类 它专门用于查询文件或目录的信息。
    // 用 filePath 这个文件路径，创建一个 QFileInfo 对象 fileInfo，以后通过它查询这个文件的信息。
    QFileInfo fileInfo(filePath);

    // 配置文件不存在
    if (!fileInfo.exists())
    {
        errorMessage = QString("Config file not found: %1").arg(filePath);

        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat); // 用 INI 格式打开这个文件。

    // settings.value(key, defaultValue) (要读取哪个配置项， 如果配置文件里找不到，就用什么默认值)
    // simulation
    // 尝试从配置文件读取 simulation/target_speed，如果配置文件没写这个参数，就继续使用 config.targetSpeed 原来的默认值，然后把结果转成 double，再存回 config.targetSpeed。
    config.targetSpeed = settings.value("simulation/target_speed", config.targetSpeed).toDouble();

    config.planningDistance = settings.value("simulation/planning_distance", config.planningDistance).toDouble();

    // controller
    config.controllerMode = settings.value("controller/mode", config.controllerMode).toString();

    config.lookAheadDistance = settings.value("controller/look_ahead", config.lookAheadDistance).toDouble();

    config.headingGain = settings.value("controller/heading_k", config.headingGain).toDouble();

    config.lateralGain = settings.value("controller/lateral_k", config.lateralGain).toDouble();

    // network
    config.plannerPort = settings.value("network/planner_port", config.plannerPort).toInt();

    if (settings.status() != QSettings::NoError)
    {
        errorMessage = QString("Failed to read config: %1").arg(filePath);

        return false;
    }

    return true;
}

bool ConfigManager::save(
    const QString &filePath,
    const AppConfig &config,
    QString &errorMessage)
{
    QFileInfo fileInfo(filePath);

    // 从 fileInfo 代表的那个文件路径里，取出这个文件所在的目录，然后保存到一个 QDir 对象 directory 里。
    QDir directory = fileInfo.dir();

    if (!directory.exists())
    {
        if (!directory.mkpath(".")) // QDir::mkpath() 是 Qt 自带的目录创建函数。 不存在就创建文件夹 创建 directory 当前代表的这个目录。
        {
            errorMessage = QString("Failed to create config directory: %1").arg(directory.absolutePath());

            return false;
        }
    }

    QSettings settings(filePath, QSettings::IniFormat);

    settings.setValue("simulation/target_speed", config.targetSpeed);

    settings.setValue("simulation/planning_distance", config.planningDistance);

    settings.setValue("controller/mode", config.controllerMode);

    settings.setValue("controller/look_ahead", config.lookAheadDistance);

    settings.setValue("controller/heading_k", config.headingGain);

    settings.setValue("controller/lateral_k", config.lateralGain);

    settings.setValue("network/planner_port", config.plannerPort);

    settings.sync(); // 现在把这些修改同步到底层存储，并检查有没有错误。

    if (settings.status() != QSettings::NoError)
    {
        errorMessage = QString("Failed to save config: %1").arg(filePath);

        return false;
    }

    return true;
}