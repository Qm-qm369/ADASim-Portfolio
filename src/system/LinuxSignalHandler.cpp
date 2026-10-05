// 把 Linux 的 SIGINT / SIGTERM 信号安全地“转接”到 Qt 的 signal/slot 体系里。
// Linux 先通知程序“你该退出了”，但真正的 Qt 清理工作不直接在 Linux signal handler 里做，而是通过 QTimer 过渡到 Qt 主线程里再处理。

#include "LinuxSignalHandler.h"

#include <QTimer>

#ifdef __linux__
#include <signal.h>
#endif

volatile std::sig_atomic_t
    LinuxSignalHandler::pendingSignal_ = 0;

LinuxSignalHandler::LinuxSignalHandler(
    QObject *parent)
    : QObject(parent)
{
    timer_ = new QTimer(this);

    connect(timer_, &QTimer::timeout,
            this, &LinuxSignalHandler::checkPendingSignal);
}

// 是整个 Linux 信号系统的注册过程
void LinuxSignalHandler::install()
{
#ifdef __linux__

    /*
    sigaction 是 Linux/POSIX 提供的结构体。
    可以把它理解成：我要告诉 Linux，收到某种信号以后应该怎么办。
    action{};表示先把这个结构体初始化。
    */
    struct sigaction action{};

    // Linux 收到对应信号时，调用 LinuxSignalHandler::handleSignal()
    action.sa_handler = &LinuxSignalHandler::handleSignal;

    sigemptyset(&action.sa_mask); // sa_mask 用来指定：signal handler 执行期间，还需要额外屏蔽哪些信号。

    action.sa_flags = 0;

    sigaction(SIGINT, &action, nullptr); // 注册SIGINT当程序收到 SIGINT 时，用刚才配置的 action 处理。

    sigaction(SIGTERM, &action, nullptr);

    // 每100ms检查一次
    timer_->start(100);

#endif
}

void LinuxSignalHandler::handleSignal(
    int signalNumber)
{
    // Signal Handler里面不操作Qt
    // 只记录一个简单整数
    pendingSignal_ = signalNumber;
}

void LinuxSignalHandler::checkPendingSignal()
{
    if (pendingSignal_ == 0)
    {
        return;
    }

    int signalNumber = static_cast<int>(pendingSignal_);

    pendingSignal_ = 0;

    emit terminationRequested(signalNumber);
}