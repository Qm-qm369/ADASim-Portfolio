#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

#ifdef Q_OS_LINUX
#include <termios.h>
#endif

class QSocketNotifier;

class SerialPort : public QObject
{
    Q_OBJECT

public:
    explicit SerialPort(QObject *parent = nullptr);
    ~SerialPort() override;

    bool openDevice(const QString &path, QString &error);
    void closeDevice();
    void sendLine(const QByteArray &line);

signals:
    void lineReceived(const QByteArray &line);
    void lineRejected(const QString &reason);
    void portError(const QString &message);

private:
    void readReady();
    void writeReady();
    void fail(const QString &message);

    int fd_ = -1;

#ifdef Q_OS_LINUX
    termios original_ {};
    bool originalValid_ = false;
#endif

    QSocketNotifier *reader_ = nullptr;
    QSocketNotifier *writer_ = nullptr;

    QByteArray line_;
    QByteArray pendingTx_;
    bool discardLine_ = false;
};
