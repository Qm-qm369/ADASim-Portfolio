#include "ObstacleDetector.h"

#include <cmath>
#include <QtGlobal>

ObstacleDetector::ObstacleDetector()
    : maxDistance_(50.0f), minPoints_(5), groundHeight_(0.5f)
{
}

ObstacleDetector::~ObstacleDetector()
{
}

void ObstacleDetector::setThreshold(float distance, int minPoints)
{
    maxDistance_ = distance; // 最大检测距离

    minPoints_ = minPoints; // 一个障碍物至少需要多少个雷达点
}

QVector<QPointF> ObstacleDetector::filterGround(const QVector<QPointF> &points)
{
    QVector<QPointF> filtered;

    // 提前给 filtered 预留和 points 一样多的空间。
    filtered.reserve(points.size());

    for (const QPointF &point : points)
    {
        filtered.append(point);
    }

    return filtered;
}

// 把一堆散乱的 QPointF 点，按照“距离近不近”分成一组一组；两点距离小于 2 米，就认为它们可能属于同一个物体。
// 把原始点云按照2 米以内能够互相连接的规则，分成多个点云簇，为后面的障碍物识别做准备。
QVector<QVector<QPointF>>
ObstacleDetector::clusterPoints(const QVector<QPointF> &points)
{
    QVector<QVector<QPointF>> clusters;

    if (points.isEmpty())
        return clusters;

    // 两点相距2米以内，认为可能属于同一个物体
    const float clusterRadius = 2.0f;

    const float radiusSq = clusterRadius * clusterRadius;

    // 创建一个和点数量一样长的布尔数组。
    QVector<bool> visited(points.size(), false);

    for (int i = 0; i < points.size(); ++i)
    {
        // 如果这个点已经属于之前的某个簇：直接跳过
        if (visited[i])
            continue;

        QVector<QPointF> cluster;

        // 这个 queue 保存的是点的下标。
        QVector<int> queue;

        queue.append(i);

        visited[i] = true;

        // head 是队列当前处理到哪里
        int head = 0;

        // BFS搜索与当前点相连的所有邻近点
        while (head < queue.size())
        {
            int idx = queue[head++];

            cluster.append(
                points[idx]);

            for (int j = 0; j < points.size(); ++j)
            {
                if (visited[j])
                    continue;

                float dx = points[idx].x() - points[j].x();

                float dy = points[idx].y() - points[j].y();

                // 使用平方距离，避免频繁sqrt
                if (dx * dx + dy * dy < radiusSq)
                {
                    visited[j] = true;

                    queue.append(j);
                }
            }
        }

        if (!cluster.isEmpty())
        {
            clusters.append(cluster);
        }
    }

    return clusters;
}

// 输入一堆雷达点 → 预处理 → 聚类 → 每个点簇算出一个障碍物 → 返回所有障碍物。
// 从低级传感器数据变成高级目标数据
QVector<Obstacle> ObstacleDetector::detect(
    const QVector<QPointF> &pointCloud)
{
    QVector<Obstacle> obstacles;

    if (pointCloud.isEmpty())
    {
        return obstacles;
    }

    // 1. 点云预处理
    QVector<QPointF> filtered =
        filterGround(pointCloud);

    // 2. 对点云进行聚类
    QVector<QVector<QPointF>> clusters =
        clusterPoints(filtered);

    // 3. 每个聚类提取一个障碍物
    for (const auto &cluster : clusters)
    {
        // 点太少，认为是噪声
        if (cluster.size() < minPoints_)
        {
            continue;
        }

        float sumX = 0.0f;
        float sumY = 0.0f;

        for (const QPointF &point : cluster)
        {
            sumX += point.x();

            sumY += point.y();
        }

        Obstacle obstacle;

        // 求整个点簇的几何中心
        obstacle.position =
            QPointF(
                sumX / cluster.size(),
                sumY / cluster.size());

        float distSq =
            obstacle.position.x() * obstacle.position.x() +
            obstacle.position.y() * obstacle.position.y();

        // 汽车的坐标就是原点
        obstacle.distance =
            std::sqrt(distSq);

        // 超出检测范围
        if (obstacle.distance > maxDistance_)
        {
            continue;
        }

        obstacle.angle =
            std::atan2(
                obstacle.position.y(),
                obstacle.position.x()); // atan2求反正切，计算角度

        obstacle.pointCount = cluster.size();

        // 这个障碍物检测结果的置信度。
        obstacle.confidence =
            qMin(1.0f, (cluster.size() / 20.0f) * (1.0f - obstacle.distance / maxDistance_));

        obstacles.append(obstacle);
    }

    return obstacles;
}