#include "SerialPort.h"

#include <QFile>
#include <QSocketNotifier>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

SerialPort::SerialPort(QObject *parent)
    : QObject(parent)
{
}

SerialPort::~SerialPort()
{
    closeDevice();
}

bool SerialPort::openDevice(
    const QString &path, QString &error)
{
    closeDevice();
    error.clear();

#ifdef Q_OS_LINUX
    const QByteArray device = QFile::encodeName(path);

    fd_ = ::open(
        device.constData(),
        O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

    if (fd_ < 0) {
        error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }

    if (tcgetattr(fd_, &original_) == -1) {
        error = QString::fromLocal8Bit(std::strerror(errno));
        closeDevice();
        return false;
    }

    originalValid_ = true;

    termios settings = original_;
    cfmakeraw(&settings);

    settings.c_cflag &=
        ~(CSIZE | PARENB | CSTOPB | CRTSCTS);

    settings.c_cflag |= CS8 | CLOCAL | CREAD;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;

    if (cfsetispeed(&settings, B115200) == -1 ||
        cfsetospeed(&settings, B115200) == -1 ||
        tcsetattr(fd_, TCSANOW, &settings) == -1) {
        error = QString::fromLocal8Bit(std::strerror(errno));
        closeDevice();
        return false;
    }

    reader_ = new QSocketNotifier(
        fd_, QSocketNotifier::Read, this);

    writer_ = new QSocketNotifier(
        fd_, QSocketNotifier::Write, this);

    writer_->setEnabled(false);

    connect(reader_, &QSocketNotifier::activated,
            this, [this](int) { readReady(); });

    connect(writer_, &QSocketNotifier::activated,
            this, [this](int) { writeReady(); });

    return true;
#else
    Q_UNUSED(path);
    error = QStringLiteral("SerialPort requires Linux");
    return false;
#endif
}

void SerialPort::closeDevice()
{
    // 先禁用通知，再关闭文件描述符。
    // deleteLater 避免在通知回调内直接销毁通知对象。
    if (reader_) {
        reader_->setEnabled(false);
        reader_->deleteLater();
        reader_ = nullptr;
    }

    if (writer_) {
        writer_->setEnabled(false);
        writer_->deleteLater();
        writer_ = nullptr;
    }

#ifdef Q_OS_LINUX
    if (fd_ >= 0) {
        if (originalValid_) {
            tcsetattr(fd_, TCSANOW, &original_);
        }

        ::close(fd_);
    }

    originalValid_ = false;
#endif

    fd_ = -1;
    line_.clear();
    pendingTx_.clear();
    discardLine_ = false;
}

void SerialPort::fail(const QString &message)
{
    closeDevice();
    emit portError(message);
}

void SerialPort::sendLine(const QByteArray &line)
{
    if (fd_ < 0) {
        return;
    }

    // 调用者传入一行正文，本函数统一追加换行。
    if (line.contains('\n') || line.contains('\r')) {
        fail(QStringLiteral("Reply contains a line break"));
        return;
    }

    if (line.size() > 4095 ||
        pendingTx_.size() + line.size() + 1 > 4096) {
        fail(QStringLiteral("Too much pending output"));
        return;
    }

    pendingTx_.append(line);
    pendingTx_.append('\n');

    writer_->setEnabled(true);
}

void SerialPort::readReady()
{
#ifdef Q_OS_LINUX
    if (fd_ < 0) {
        return;
    }

    char buffer[128];
    const ssize_t count = ::read(fd_, buffer, sizeof(buffer));

    if (count == 0) {
        fail(QStringLiteral("Serial input closed"));
        return;
    }

    if (count < 0) {
        if (errno == EAGAIN ||
            errno == EWOULDBLOCK ||
            errno == EINTR) {
            return;
        }

        fail(QStringLiteral("read: ") +
             QString::fromLocal8Bit(std::strerror(errno)));
        return;
    }

    for (ssize_t i = 0; i < count; ++i) {
        const char ch = buffer[i];

        if (ch == '\n') {
            const bool rejected = discardLine_;
            QByteArray completed = line_;

            line_.clear();
            discardLine_ = false;

            if (rejected) {
                emit lineRejected(
                    QStringLiteral("line-too-long"));
            } else {
                if (completed.endsWith('\r')) {
                    completed.chop(1);
                }

                emit lineReceived(completed);
            }

            // 接收信号的代码可能关闭了串口。
            if (fd_ < 0) {
                return;
            }
        } else if (!discardLine_) {
            if (line_.size() < 64) {
                line_.append(ch);
            } else {
                line_.clear();
                discardLine_ = true;
            }
        }
    }
#endif
}

void SerialPort::writeReady()
{
#ifdef Q_OS_LINUX
    if (fd_ < 0) {
        return;
    }

    if (pendingTx_.isEmpty()) {
        writer_->setEnabled(false);
        return;
    }

    const ssize_t count = ::write(
        fd_, pendingTx_.constData(), pendingTx_.size());

    if (count > 0) {
        pendingTx_.remove(0, static_cast<int>(count));
    } else if (count < 0) {
        if (errno == EAGAIN ||
            errno == EWOULDBLOCK ||
            errno == EINTR) {
            return;
        }

        fail(QStringLiteral("write: ") +
             QString::fromLocal8Bit(std::strerror(errno)));
        return;
    } else {
        fail(QStringLiteral("Write made no progress"));
        return;
    }

    if (pendingTx_.isEmpty()) {
        writer_->setEnabled(false);
    }
#endif
}
