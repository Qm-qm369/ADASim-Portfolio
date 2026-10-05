#ifndef PATHPREDICTOR_H
#define PATHPREDICTOR_H

#include <QVector>
#include <QPointF>

// 一次轨迹预测的完整结果
struct PredictedPath
{
    // 未来预测位置
    QVector<QPointF> path;

    // 预测可信度
    float confidence = 0.0f;

    // 预测未来多少秒
    int horizon = 0;
};

class PathPredictor
{
public:
    PathPredictor();
    ~PathPredictor();

    // 根据历史轨迹预测未来运动
    [[nodiscard]]
    PredictedPath predict(const QVector<QPointF> &trajectory, int seconds = 3);

    // 设置运行频率和速度
    void setParameters(float frequency, float speed);

private:
    // 匀速运动模型
    QPointF constantVelocityModel(
        const QPointF &lastPoint,
        const QPointF &velocity,
        float futureTime);

private:
    // 默认10Hz
    float frequency_ = 10.0f;

    float speed_ = 0.0f;
};

#endif