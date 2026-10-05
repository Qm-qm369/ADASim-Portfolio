#include "PathPredictor.h"

#include <cmath>
#include <QtGlobal>

PathPredictor::PathPredictor()
    : frequency_(10.0f), speed_(0.0f)
{
}

PathPredictor::~PathPredictor()
{
}

QPointF PathPredictor::constantVelocityModel(
    const QPointF &lastPoint,
    const QPointF &velocity,
    float futureTime)
{
    float predictedX =
        lastPoint.x() + velocity.x() * futureTime;

    float predictedY =
        lastPoint.y() + velocity.y() * futureTime;

    return QPointF(
        predictedX,
        predictedY);
}

void PathPredictor::setParameters(
    float frequency,
    float speed)
{
    // 防止 frequency = 0
    frequency_ =
        (frequency > 0.001f)
            ? frequency
            : 10.0f;

    speed_ = speed;
}

// 先用最近两帧的位置算出“当前速度”，
// 然后假设未来速度一直不变，用 位置 = 当前的位置 + 速度 × 时间 一直往后推。

PredictedPath PathPredictor::predict(
    const QVector<QPointF> &trajectory,
    int seconds)
{
    PredictedPath result;

    result.horizon = seconds;

    // 至少需要两个历史位置
    // 才能计算车辆速度
    if (trajectory.size() < 2)
    {
        result.confidence = 0.0f;

        return result;
    }

    // 当前点
    const QPointF &lastPoint = trajectory.last();

    // 上一帧位置
    const QPointF &prevPoint = trajectory[trajectory.size() - 2];

    // 防止频率为0
    float safeFreq = (frequency_ > 0.001f) ? frequency_ : 10.0f;

    // 两帧之间的时间
    float dt = 1.0f / safeFreq;

    // =====================================
    // 1. 根据最近两帧计算速度
    // =====================================

    QPointF velocity(
        (lastPoint.x() - prevPoint.x()) / dt,

        (lastPoint.y() - prevPoint.y()) / dt);

    // 当前标量速度
    speed_ =
        std::hypot(velocity.x(), velocity.y());

    // =====================================
    // 2. 计算未来需要多少个点
    // =====================================

    int totalSteps = seconds * static_cast<int>(safeFreq);

    result.path.reserve(totalSteps); // 提前给result.path预留 30 个点的内存。

    // =====================================
    // 3. 逐帧预测未来位置
    // =====================================

    for (int step = 1; step <= totalSteps; ++step)
    {
        float futureTime = step * dt;

        QPointF predicted =
            constantVelocityModel(
                lastPoint,
                velocity,
                futureTime);

        result.path.append(predicted);
    }

    // =====================================
    // 4. 简单计算预测置信度
    // =====================================

    float historyFactor = qMin(1.0f, trajectory.size() / 50.0f);

    float speedFactor = (speed_ > 0.5f) ? 1.0f : 0.5f;

    result.confidence = historyFactor * speedFactor;

    return result;
}