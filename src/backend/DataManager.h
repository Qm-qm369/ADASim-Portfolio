/*
 类似于数据总线思想 这就是个中间层
DataLoader 产生数据
        ↓
DataManager 保存数据
        ↓
MainWindow 显示
*/

#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QByteArray>
#include <QString>

#include "algorithm/PathPredictor.h"
#include "algorithm/ObstacleDetector.h"

class DataManager : public QObject
{
    Q_OBJECT

public:
    explicit DataManager(QObject *parent = nullptr);
    ~DataManager();

signals:

    // 预测出的未来轨迹
    void pathPredicted(const QVector<QPointF> &predictedTrajectory);

    void obstaclesDetected(const QVector<QPointF> &obstaclePositions);

    void mergedPointCloudReady(const QVector<QPointF> &points);

    // V1.8：当前正前方最近障碍物距离
    // -1 表示当前没有前方障碍物
    void frontObstacleDistanceUpdated(double distance);

    // 给 UI 和算法发送最新车辆状态
    void vehicleStateUpdated(
        double x,
        double y,
        double yaw);

    // 把规划输入数据交给网络模块
    void plannerDataReady(const QByteArray &data, const QString &runId,
                          qint64 producedAtMs);

public slots:
    void onPointCloudReceived(const QVector<QPointF> &points);

    // 接收 DataLoader 数据
    void updateVehicleState(
        double x,
        double y,
        double yaw);

    void onUserObstacleAdded(
        double x,
        double y);

    void onVehiclePositionReceived(
        double x,
        double y,
        double yaw);

    void onSimulationFrame(
        double x,
        double y,
        double yaw,
        const QVector<QPointF> &points,
        const QString &runId,
        qint64 producedAtMs);

private:
    // 轨迹预测算法
    PathPredictor pathPredictor_;

    // 保存车辆历史位置
    QVector<QPointF> historyTrajectory_;

    // 点云聚类算法
    ObstacleDetector obstacleDetector_;

    // 用户右键添加的真值障碍物，全局坐标
    QVector<QPointF> userObstacles_;

    // V0.7车辆状态
    // 保存 DataLoader 传进来的当前车辆状态，然后再通过 vehicleStateUpdated 转发出去。
    double vehicleX_ = 0.0;
    double vehicleY_ = 0.0;
    double vehicleYaw_ = 0.0;

    // V0.8感知计算使用的当前自车位姿 做障碍物世界坐标和自车局部坐标之间的转换。
    double lastEgoX_ = 0.0;
    double lastEgoY_ = 0.0;
    double lastEgoYaw_ = 0.0;

    QString plannerInputRunId_;
    qint64 plannerInputTimeMs_ = 0;
};

#endif