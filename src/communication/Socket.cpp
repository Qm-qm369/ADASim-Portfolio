#include "Socket.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QPointer>
#include <QTimer>

#include <QTcpServer>
#include <QTcpSocket>

SocketServer::SocketServer(
    QObject *parent)
    : QObject(parent)
{
    // 创建 TCP 服务器对象。
    server_ = new QTcpServer(this);

    // Qt 的 TCP 服务器发现有新客户端连接时，自动执行我们自己写的 onNewConnection()。
    connect(server_, &QTcpServer::newConnection,
            this, &SocketServer::onNewConnection);
}

SocketServer::~SocketServer()
{
    stop();
}

bool SocketServer::startTcpServer(
    quint16 port)
{
    if (server_->isListening())
    {
        return true;
    }

    bool success = server_->listen(QHostAddress::LocalHost, port);

    if (!success)
    {
        // server_->errorString() Qt 网络类给你的真正错误描述
        emit networkError(QString("TCP listen failed: %1").arg(server_->errorString()));
    }

    return success;
}

void SocketServer::stop()
{
    server_->close();
    closeClient();
}

// C++ 端向已经连接的 Python 客户端发送数据。
void SocketServer::sendToClient(const QByteArray &data)
{
    QTcpSocket *socket = client_;
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    // 当前调用约定是一次传入一条、已经带 LF 的完整消息。
    if (data.isEmpty() || !data.endsWith('\n') ||
        data.count('\n') != 1 || data.size() - 1 > MaxLineBytes)
    {
        closeClient("Outgoing Planner message has invalid framing or length");
        return;
    }

    if (socket->bytesToWrite() + data.size() > MaxPendingWriteBytes)
    {
        closeClient("Planner write backlog exceeds 262144 bytes");
        return;
    }

    const qint64 accepted = socket->write(data);
    if (accepted != data.size())
    {
        closeClient(QString("TCP write incomplete: %1/%2")
                        .arg(accepted)
                        .arg(data.size()));
    }
    // 由事件循环发送，不调用阻塞式 waitForBytesWritten。
}

// 只允许一个客户端接入
void SocketServer::onNewConnection()
{
    // QTcpServer 可能一次积压多个连接，所以用 while 全部取出。nextPendingConnection() 拿到一个已建立的 QTcpSocket。拿不到就跳过。
    while (server_->hasPendingConnections())
    {
        QTcpSocket *socket = server_->nextPendingConnection();
        if (!socket)
            continue;

        if (client_ &&
            client_->state() == QAbstractSocket::ConnectedState)
        {
            socket->abort();
            socket->deleteLater();
            emit networkError("A second Planner connection was rejected");
            continue;
        }

        closeClient();
        client_ = socket;
        socket->setReadBufferSize(MaxLineBytes + 1);

        connect(socket, &QTcpSocket::readyRead,
                this, &SocketServer::onReadyRead);

        connect(socket, &QTcpSocket::disconnected,
                this, [this, socket]()
                {
                    if (client_ == socket)
                        closeClient(); });

        // 兼容 Ubuntu 22.04 的 Qt 5.15，以及较旧 Qt 5。
        const auto onError =
            [this, socket](QAbstractSocket::SocketError error)
        {
            if (client_ != socket)
                return;
            if (error == QAbstractSocket::RemoteHostClosedError)
                return; // disconnected 统一处理正常远端关闭。
            closeClient(QString("TCP error: %1")
                            .arg(socket->errorString()));
        };
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(socket, &QAbstractSocket::errorOccurred, this, onError);
#else
        connect(socket,
                QOverload<QAbstractSocket::SocketError>::of(
                    &QAbstractSocket::error),
                this, onError);
#endif
        emit clientConnected();
    }
}

void SocketServer::onReadyRead()
{
    QTcpSocket *socket = client_;
    if (!socket)
        return;

    /*
    三个条件同时成立才继续读：
    client_ == socket：还是刚才那个连接，没被 closeClient() / 新连接替换
    canReadLine()：缓冲里已经有一整行（含 \n）
    lines < 64：本轮最多处理 64 行，把 CPU 还给事件循环，避免一次来几万行把 UI/网络线程卡死
    */
    int lines = 0;
    while (client_ == socket && socket->canReadLine() && lines < 64)
    {
        const QByteArray wire = socket->readLine(MaxLineBytes + 2); // 最多读「上限 + 换行 + 一点余量」。

        // 判定非法行：
        // 读出来却不以 \n 结尾 → 行被截断，超长了
        // 去掉换行后长度仍 > MaxLineBytes → 超长
        if (!wire.endsWith('\n') || wire.size() - 1 > MaxLineBytes)
        {
            closeClient("Planner line exceeds 65536 bytes");
            return;
        }
        ++lines;
        const QByteArray payload = wire.left(wire.size() - 1).trimmed();
        if (!payload.isEmpty())
            emit dataReceived(payload);
    }

    if (client_ != socket)
        return;

    // 缓冲里还凑不成一行，但未完成的数据已经超过 MaxLineBytes。
    // 说明对端在发一条没有 \n 的超长垃圾，也断开。
    if (!socket->canReadLine() && socket->bytesAvailable() > MaxLineBytes)
    {
        closeClient("Planner unterminated line exceeds 65536 bytes");
        return;
    }

    if (socket->canReadLine())
    {
        // 已有数据未必再次触发 readyRead，因此主动安排下一轮。
        // QPointer 和身份比较防止回调落到销毁对象或新连接上。
        const QPointer<QTcpSocket> expected(socket);
        QTimer::singleShot(0, this, [this, expected]()
                           {
                                if (expected && client_ == expected.data())
                                    onReadyRead(); });
    }
}

quint16 SocketServer::listeningPort() const
{

    return server_->serverPort(); // 返回端口。
}

void SocketServer::closeClient(
    const QString &reason)
{

    QTcpSocket *socket = client_;

    if (!socket)
        return;

    client_ = nullptr;

    QObject::disconnect(socket, nullptr, this, nullptr);

    socket->abort(); // 立即关闭TCP。

    socket->deleteLater(); // Qt安全释放。

    if (!reason.isEmpty())
    {
        emit networkError(reason);
    }

    emit clientDisconnected();
}
