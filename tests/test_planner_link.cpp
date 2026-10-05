#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include "communication/PlannerLink.h"

static QJsonObject request(PlannerLink &link, QSignalSpy &sent, qint64 at)
{
    link.onPerception("{\"type\":\"OBSTACLES\",\"data\":[]}", "run-A", at);
    return QJsonDocument::fromJson(sent.last().at(0).toByteArray()).object();
}

static QByteArray reply(QJsonObject object, double offset = 1.75)
{
    object.remove("data");
    object["type"] = "CONTROL";
    object["steer_offset"] = offset;
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

class PlannerLinkTest : public QObject
{
    Q_OBJECT
private slots:
    void validDuplicateAndOrder();
    void invalidFieldsDoNotConsumeRequest();
    void ageAndSessionBoundaries();
};

void PlannerLinkTest::validDuplicateAndOrder()
{
    qint64 now = 1000;
    PlannerLink link(nullptr, [&]
                     { return now; });
    QSignalSpy sent(&link, &PlannerLink::sendRequested);
    QSignalSpy accepted(&link, &PlannerLink::controlReady);
    link.setRun("run-A");
    link.onConnected();
    const auto first = request(link, sent, now);
    const auto second = request(link, sent, now);
    link.onReply(reply(second));
    QCOMPARE(accepted.count(), 1);
    link.onReply(reply(second));
    link.onReply(reply(first));
    QCOMPARE(accepted.count(), 1);
}

void PlannerLinkTest::invalidFieldsDoNotConsumeRequest()
{
    qint64 now = 1000;
    PlannerLink link(nullptr, [&]
                     { return now; });
    QSignalSpy sent(&link, &PlannerLink::sendRequested);
    QSignalSpy accepted(&link, &PlannerLink::controlReady);
    QSignalSpy errors(&link, &PlannerLink::protocolError);
    link.setRun("run-A");
    link.onConnected();
    const auto original = request(link, sent, now);
    auto changed = original;
    changed["protocol_version"] = 2;
    link.onReply(reply(changed));
    changed = original;
    changed["frame_id"] = 1.5;
    link.onReply(reply(changed));
    changed["frame_id"] = 99999;
    link.onReply(reply(changed));
    link.onReply(reply(original, 99));
    link.onReply("[]");
    QCOMPARE(accepted.count(), 0);
    QCOMPARE(errors.count(), 5);
    link.onReply(reply(original));
    QCOMPARE(accepted.count(), 1);
}

void PlannerLinkTest::ageAndSessionBoundaries()
{
    qint64 now = 1000;
    PlannerLink link(nullptr, [&]
                     { return now; });
    QSignalSpy sent(&link, &PlannerLink::sendRequested);
    QSignalSpy accepted(&link, &PlannerLink::controlReady);
    link.setRun("run-A");
    link.onConnected();
    auto message = request(link, sent, now);
    now = 1499;
    link.onReply(reply(message));
    QCOMPARE(accepted.count(), 1);
    message = request(link, sent, now);
    now = 1999;
    link.onReply(reply(message)); // 恰好500ms，拒绝
    QCOMPARE(accepted.count(), 1);
    message = request(link, sent, now);
    link.onDisconnected();
    link.onConnected();
    link.onReply(reply(message));
    QCOMPARE(accepted.count(), 1);
    message = request(link, sent, now);
    link.setRun("");
    link.onReply(reply(message));
    link.setRun("run-B");
    const int before = sent.count();
    link.onPerception("{\"type\":\"OBSTACLES\",\"data\":[]}", "run-A", now);
    QCOMPARE(sent.count(), before); // 后台上一轮输入被丢弃
    link.onReply(reply(message));
    QCOMPARE(accepted.count(), 1);
}

QTEST_GUILESS_MAIN(PlannerLinkTest)
#include "test_planner_link.moc"