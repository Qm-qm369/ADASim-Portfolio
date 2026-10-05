#include "DataLoader.h"

#include <QTimer>
#include <QDebug>
#include <QThread>

DataLoader::DataLoader(const QString &dataPath,
                       QObject *parent)
    : QObject(parent), dataPath_(dataPath), isRunning_(false), timer_(new QTimer(this))
{
    // QVector<QPointF> 后面需要跨线程传输
    // 把 QVector<QPointF> 这种数据类型登记到 Qt 的“类型系统”里，让 Qt 知道这种类型。
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");

    /*
    QObject::connect(
    const QObject *sender,        // 参数1：信号发送者对象指针
    const char *signal,           // 参数2：信号（函数地址）
    const QObject *receiver,     // 参数3：接收者对象指针
    const char *method,          // 参数4：槽函数（函数地址）
    Qt::ConnectionType type = Qt::AutoConnection // 参数5：连接类型（可选，大部分情况不用写）
    );

    */
    // QTimer 每次 timeout 都执行 loadNextFrame()
    connect(timer_, &QTimer::timeout,
            this, &DataLoader::loadNextFrame);
}

DataLoader::~DataLoader()
{
    stop();
}

/**
 * 启动仿真
 */
void DataLoader::start()
{
    qDebug() << "DataLoader::start thread ="
             << QThread::currentThread();
    if (QThread::currentThread() != this->thread())
    {
        return;
    }

    if (!isRunning_)
    {
        isRunning_ = true;
        timer_->start(100);

        emit statusUpdate("正在运行：仿真推演中...");
    }
}

/**
 * 暂停仿真
 */
void DataLoader::pause()
{
    if (isRunning_)
    {
        isRunning_ = false;
        timer_->stop();

        emit statusUpdate("仿真已暂停");
    }
}

/**
 * 停止并重置
 */
void DataLoader::stop()
{
    isRunning_ = false;

    if (timer_->isActive())
    {
        timer_->stop();
    }

    emit statusUpdate("引擎已停止并重置");
}

/*
    彻底不管  X是多少 Y是多少 Yaw是多少
    它只说：下一帧时间到了。
*/
void DataLoader::loadNextFrame()
{
    if (!isRunning_)
    {
        return;
    }

    emit simulationTick(QVector<QPointF>());
}