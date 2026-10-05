#ifndef LINUXSIGNALHANDLER_H
#define LINUXSIGNALHANDLER_H

#include <QObject>
#include <csignal>

class QTimer;

class LinuxSignalHandler
    : public QObject
{
    Q_OBJECT

public:
    explicit LinuxSignalHandler(QObject *parent = nullptr);

    // 安装SIGINT / SIGTERM处理函数
    // SIGINT 可以理解成：用户在终端里主动中断程序。 Ctrl+C
    // SIGTERM 可以理解成：系统或其他进程请求你正常退出。 kill 12345
    void install();

signals:

    // 回到Qt事件循环以后再发出
    void terminationRequested(int signalNumber);

private slots:

    void checkPendingSignal();

private:
    static void handleSignal(int signalNumber);

    static volatile std::sig_atomic_t pendingSignal_; // 是 C/C++ 标准库提供的一种整数类型，专门给 signal handler（信号处理函数） 这种场景用的。

    QTimer *timer_ = nullptr;
};

#endif