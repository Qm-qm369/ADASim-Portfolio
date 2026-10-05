#ifndef OBSTACLEDETECTOR_H
#define OBSTACLEDETECTOR_H

#include <QVector>
#include <QPointF>

struct Obstacle
{
    // 障碍物中心，自车局部坐标
    QPointF position;

    // 距离自车的距离
    float distance = 0.0f;

    // 相对自车角度
    float angle = 0.0f;

    // 可信度
    float confidence = 0.0f;

    // 聚类包含的雷达点数
    int pointCount = 0;
};

class ObstacleDetector
{
public:
    ObstacleDetector();
    ~ObstacleDetector();

    // [[nodiscard]]这个函数的返回值很重要，调用它以后，不建议你把返回结果直接丢掉。
    [[nodiscard]]
    QVector<Obstacle> detect(const QVector<QPointF> &pointCloud);

    void setThreshold(float maxDistance, int minPoints);

private:
    QVector<QPointF> filterGround(const QVector<QPointF> &points);

    QVector<QVector<QPointF>> clusterPoints(const QVector<QPointF> &points);

private:
    float maxDistance_ = 50.0f;

    int minPoints_ = 3;

    float groundHeight_ = -1.0f;
};

#endif