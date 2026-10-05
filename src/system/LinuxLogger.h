/*
POSIX/Linux syslog() 系统日志接口。

在 Linux 环境下，把 ADASim 的日志写入 Linux 系统日志 syslog；
在非 Linux 环境（比如 Windows 开发机）下，使用 Qt 自带的 qDebug/qWarning/qCritical 输出。
*/

#ifndef LINUXLOGGER_H
#define LINUXLOGGER_H

#include <QString>

class LinuxLogger
{
public:
    static void init();
    static void shutdown();

    static void info(const QString &message);

    static void warning(const QString &message);

    static void error(const QString &message);
};

#endif