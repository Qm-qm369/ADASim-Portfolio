#ifndef DATALOADER_H
#define DATALOADER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPointF>

class QTimer;

class DataLoader : public QObject
{
    Q_OBJECT

public:
    // explicit 禁止构造函数的隐式类型转换，只允许显式调用构造。只对单参数（或者除第一个参数外都有默认值）**的构造函数生效。
    explicit DataLoader(const QString &dataPath,
                        QObject *parent = nullptr);

    ~DataLoader();

signals:

    // 向界面发送运行状态
    void statusUpdate(const QString &status);

    // V1.6：每100ms通知主控制流程执行一帧
    void simulationTick(const QVector<QPointF> &points);

public slots:
    void start();
    void pause();
    void stop();

private slots:
    // 定时器每次触发时产生一帧数据
    void loadNextFrame();

private:
    QString dataPath_;

    bool isRunning_ = false;

    QTimer *timer_ = nullptr;
};

#endif // DATALOADER_H