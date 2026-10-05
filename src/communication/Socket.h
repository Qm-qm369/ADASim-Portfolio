#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QByteArray>
#include <QtGlobal>

class QTcpServer;
class QTcpSocket;

/**
 * V0.10
 *
 * 简单 TCP Server：
 * C++ 负责监听
 * Python 负责连接
 */
class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QObject *parent = nullptr);
    ~SocketServer();

    // 启动 TCP Server
    bool startTcpServer(quint16 port);

    // 新增
    quint16 listeningPort() const;

    // 停止服务器
    void stop();

public slots:

    // C++ -> Python
    void sendToClient(const QByteArray &data);

signals:

    // Python -> C++
    void dataReceived(const QByteArray &data);
    void clientConnected();
    void clientDisconnected();
    void networkError(const QString &errorMessage);

private slots:

    // Python连接进来了
    void onNewConnection();

    // Python发数据过来了
    void onReadyRead();

private:
    void closeClient(
        const QString &reason = QString());

private:
    QTcpServer *server_ = nullptr; // 监听端口、等待客户端连接。

    QTcpSocket *client_ = nullptr; // 和某一个具体的 TCP 客户端进行数据收发。

    static constexpr qint64 MaxLineBytes = 64 * 1024; // 单条TCP消息最大：64KB 防止恶意或者异常数据：

    static constexpr qint64 MaxPendingWriteBytes = 256 * 1024; // 限制：发送缓存：256KB 防止一直发送不接收
};

#endif