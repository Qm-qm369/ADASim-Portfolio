#include <QtTest>
#include <QHostAddress>
#include <QTcpSocket>
#include "communication/Socket.h"

class SocketServerTest : public QObject
{
    Q_OBJECT

private slots:
    void fragmentedAndCoalescedMessages()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy messages(&server, &SocketServer::dataReceived);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);

        client.write("{\"type\":");
        client.flush();
        QTest::qWait(30);
        QCOMPARE(messages.count(), 0);

        client.write("\"CONTROL\"}\n{\"second\":2}\n");
        QTRY_COMPARE(messages.count(), 2);
        QCOMPARE(messages.at(0).at(0).toByteArray(),
                 QByteArray("{\"type\":\"CONTROL\"}"));
        QCOMPARE(messages.at(1).at(0).toByteArray(),
                 QByteArray("{\"second\":2}"));
    }

    void maximumLengthIsAccepted()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy messages(&server, &SocketServer::dataReceived);
        QSignalSpy errors(&server, &SocketServer::networkError);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);

        const QByteArray payload(65536, 'x');
        client.write(payload + '\n');
        QTRY_COMPARE(messages.count(), 1);
        QCOMPARE(messages.at(0).at(0).toByteArray(), payload);
        QCOMPARE(errors.count(), 0);
    }

    void oversizedUnterminatedMessageDisconnects()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy messages(&server, &SocketServer::dataReceived);
        QSignalSpy errors(&server, &SocketServer::networkError);
        QSignalSpy disconnected(&server, &SocketServer::clientDisconnected);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);

        client.write(QByteArray(65537, 'x'));
        QTRY_COMPARE(disconnected.count(), 1);
        QCOMPARE(messages.count(), 0);
        QVERIFY(errors.count() >= 1);
    }

    void newConnectionDoesNotInheritPartialMessage()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy disconnected(&server, &SocketServer::clientDisconnected);
        QSignalSpy messages(&server, &SocketServer::dataReceived);

        QTcpSocket first;
        first.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);
        first.write("unfinished");
        first.flush();
        QTest::qWait(30);
        first.abort();
        QTRY_COMPARE(disconnected.count(), 1);

        QTcpSocket second;
        second.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 2);
        second.write("fresh\n");
        QTRY_COMPARE(messages.count(), 1);
        QCOMPARE(messages.at(0).at(0).toByteArray(), QByteArray("fresh"));
    }

    void rejectsSecondClientAndKeepsFirst()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy messages(&server, &SocketServer::dataReceived);
        QSignalSpy errors(&server, &SocketServer::networkError);

        QTcpSocket first;
        first.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);
        QTcpSocket second;
        second.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_VERIFY(errors.count() >= 1);
        QCOMPARE(connected.count(), 1);
        first.write("still-first\n");
        QTRY_COMPARE(messages.count(), 1);
        QCOMPARE(messages.at(0).at(0).toByteArray(), QByteArray("still-first"));
    }

    void rejectsOversizedOutgoingMessage()
    {
        SocketServer server;
        QVERIFY(server.startTcpServer(0));
        QSignalSpy connected(&server, &SocketServer::clientConnected);
        QSignalSpy disconnected(&server, &SocketServer::clientDisconnected);
        QSignalSpy errors(&server, &SocketServer::networkError);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.listeningPort());
        QTRY_COMPARE(connected.count(), 1);
        server.sendToClient(QByteArray(65537, 'x') + '\n');
        QCOMPARE(disconnected.count(), 1);
        QCOMPARE(errors.count(), 1);
    }
};

QTEST_GUILESS_MAIN(SocketServerTest)
#include "test_socket_server.moc"