#pragma once

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <functional>

class PlannerLink : public QObject
{
    Q_OBJECT
public:
    explicit PlannerLink(QObject *parent = nullptr,
                         std::function<qint64()> now = {});

signals:
    void sendRequested(const QByteArray &wire);
    void controlReady(double offset);
    void protocolError(const QString &reason);

public slots:
    void setRun(const QString &runId);
    void onConnected();
    void onDisconnected();
    void onPerception(const QByteArray &json, const QString &runId,
                      qint64 producedAtMs);
    void onReply(const QByteArray &json);

private:
    void rotateSession();
    std::function<qint64()> now_;
    bool connected_ = false;
    QString runId_;
    QString sessionId_;
    int nextFrame_ = 0;
    int lastAccepted_ = 0;
    QMap<int, qint64> pending_;
    static constexpr qint64 MaxAgeMs = 500;
    static constexpr int MaxPending = 64;
};