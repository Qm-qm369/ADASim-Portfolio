#include "PlannerLink.h"
#include "PlannerClock.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUuid>
#include <cmath>
#include <limits>
#include <utility>

PlannerLink::PlannerLink(QObject *parent, std::function<qint64()> now)
    : QObject(parent),
      now_(now ? std::move(now) : plannerNowMs)
{
}

void PlannerLink::rotateSession() // 作废旧账，开一本新账
{
    // QUuid::createUuid() 生成随机 UUID；WithoutBraces 得到 a1b2c3d4-... 这种形式，没有 { }，方便直接塞进 JSON。
    sessionId_ = QUuid::createUuid().toString(QUuid::WithoutBraces); // QUuid是 Qt 提供的 UUID（通用唯一识别码）封装类，在<QUuid>头文件里，用来生成、解析、比较 UUID。
    nextFrame_ = 0;
    lastAccepted_ = 0;
    pending_.clear();
}

void PlannerLink::setRun(const QString &runId)
{
    runId_ = runId;
    rotateSession();
}

void PlannerLink::onConnected()
{
    connected_ = true;
    rotateSession();
}

void PlannerLink::onDisconnected()
{
    connected_ = false;
    rotateSession();
}

// 这是「感知已经做好、准备发给 Python」的出口。DataManager 组好 OBSTACLES JSON 后进这里；通过检查才会盖章发出去。
void PlannerLink::onPerception(const QByteArray &json,
                               const QString &runId, qint64 producedAtMs)
{
    if (!connected_ || runId_.isEmpty() || runId != runId_)
        return;

    const qint64 now = now_();
    if (producedAtMs > now || now - producedAtMs >= MaxAgeMs)
    {
        emit protocolError("stale perception input");
        return;
    }

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        emit protocolError("invalid local perception JSON");
        return;
    }
    QJsonObject root = doc.object(); // 把已经解析好的 JSON 文档，取出根对象，拷到一个可改的 QJsonObject 里。
    if (root.value("type").toString() != "OBSTACLES" ||
        !root.value("data").isArray())
    {
        emit protocolError("invalid local perception schema");
        return;
    }

    // 打扫 pending，腾名额
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        if (now - it.value() >= MaxAgeMs)
            it = pending_.erase(it);
        else
            ++it;
    }
    if (nextFrame_ == std::numeric_limits<int>::max())
        rotateSession();
    while (pending_.size() >= MaxPending)
        pending_.erase(pending_.begin()); // 明确淘汰最早未完成请求

    // 盖章并记住，然后发给 Socket
    const int id = ++nextFrame_;
    root["protocol_version"] = 1;
    root["session_id"] = sessionId_;
    root["frame_id"] = id;
    pending_.insert(id, producedAtMs);
    emit sendRequested(QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n');
}

void PlannerLink::onReply(const QByteArray &json)
{
    const auto reject = [this](const QString &reason)
    { emit protocolError(reason); };

    if (!connected_ || runId_.isEmpty())
    {
        reject("CONTROL while inactive");
        return;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        reject("CONTROL invalid JSON object");
        return;
    }
    const auto root = doc.object();
    const auto version = root.value("protocol_version");
    if (!version.isDouble() || version.toDouble() != 1.0)
    {
        reject("CONTROL unsupported protocol_version");
        return;
    }
    if (root.value("type").toString() != "CONTROL")
    {
        reject("CONTROL invalid type");
        return;
    }
    const auto session = root.value("session_id");
    if (!session.isString() || session.toString() != sessionId_)
    {
        reject("CONTROL old or invalid session_id");
        return;
    }
    const auto frame = root.value("frame_id");
    const double number = frame.toDouble(-1);
    if (!frame.isDouble() || !std::isfinite(number) ||
        number < 1 || number > 2147483647.0 || std::floor(number) != number)
    {
        reject("CONTROL frame_id must be a positive int32");
        return;
    }
    const int id = static_cast<int>(number);
    if (id <= lastAccepted_ || !pending_.contains(id))
    {
        reject("CONTROL duplicate, out-of-order or unknown frame_id");
        return;
    }
    const qint64 producedAt = pending_.value(id);
    const qint64 now = now_();
    if (producedAt > now || now - producedAt >= MaxAgeMs)
    {
        pending_.remove(id);
        reject("CONTROL expired");
        return;
    }
    const auto value = root.value("steer_offset");
    const double offset = value.toDouble();
    if (!value.isDouble() || !std::isfinite(offset) || std::abs(offset) > 3.5)
    {
        reject("CONTROL steer_offset must be finite and within [-3.5, 3.5] m");
        return;
    }

    lastAccepted_ = id;
    while (!pending_.isEmpty() && pending_.firstKey() <= id)
        pending_.erase(pending_.begin());
    emit controlReady(offset);
}