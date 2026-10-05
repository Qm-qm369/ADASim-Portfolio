#include "LinuxLogger.h"

#include <QDebug>

#ifdef __linux__
#include <syslog.h>
#endif

void LinuxLogger::init()
{
#ifdef __linux__ // 这是条件编译，只有在 Linux 编译时才编译 Linux 专用代码。

    openlog("ADASim", LOG_PID | LOG_NDELAY, LOG_USER);

#else

    qInfo() << "LinuxLogger initialized";

#endif
}

void LinuxLogger::shutdown()
{
#ifdef __linux__

    closelog();

#endif
}

void LinuxLogger::info(
    const QString &message)
{
#ifdef __linux__

    QByteArray data = message.toUtf8();

    syslog(LOG_INFO, "%s", data.constData());

#else

    qInfo().noquote() << message;

#endif
}

void LinuxLogger::warning(
    const QString &message)
{
#ifdef __linux__

    QByteArray data = message.toUtf8();

    syslog(LOG_WARNING, "%s", data.constData());

#else

    qWarning().noquote() << message;

#endif
}

void LinuxLogger::error(const QString &message)
{
#ifdef __linux__

    QByteArray data = message.toUtf8();

    syslog(LOG_ERR, "%s", data.constData());

#else

    qCritical().noquote() << message;

#endif
}