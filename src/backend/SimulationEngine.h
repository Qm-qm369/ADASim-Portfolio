#ifndef SIMULATIONENGINE_H
#define SIMULATIONENGINE_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>

#include "algorithm/VehicleModel.h"
#include "algorithm/TrajectoryController.h"
#include "algorithm/PurePursuitController.h"
#include "algorithm/LongitudinalController.h"
#include "backend/SimulationRecorder.h"
#include "test/TestEvaluator.h"

class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    explicit SimulationEngine(QObject *parent = nullptr);

    // V2.3：初始化仿真参数
    void configure(
        double targetSpeed,
        double planningDistance,
        double lookAheadDistance,
        double headingGain,
        double lateralGain,
        bool usePurePursuit);

    bool startRecording(
        const QString &filePath);

    TestResult testResult(
        AebExpectation expectation = AebExpectation::Any) const;

signals:

    // 当前车辆状态继续交给 DataManager
    // 对应原来 MainWindow::simulationFrameReady
    void simulationFrameReady(
        double x,
        double y,
        double yaw,
        const QVector<QPointF> &points,
        const QString &runId,
        qint64 producedAtMs);

    // 一帧核心计算完成，通知 GUI 更新显示
    void frameUpdated(
        const SimulationFrame &frame,
        int currentIndex,
        bool targetSettled);

    // 横向规划轨迹改变
    void plannedTrajectoryUpdated(
        const QVector<QPointF> &trajectory);

    // 从历史回放恢复实时显示时使用
    void liveVehicleStateReady(
        double x,
        double y,
        double yaw);

    // 返回历史回放帧
    void replayFrameReady(
        int index,
        int maxIndex,
        const SimulationFrame &frame);

    void plannerRunChanged(const QString &runId);

public slots:

    void start();

    void pause();

    void stop();

    // 对应原来的 MainWindow::onSimulationTick()
    void onSimulationTick(
        const QVector<QPointF> &points);

    // 对应原来的 MainWindow::onLateralControlReceived()
    void onLateralControlReceived(
        double offset);

    // 对应原来的 MainWindow::onFrontObstacleDistanceUpdated()
    void onFrontObstacleDistanceUpdated(
        double distance);

    // GUI 修改参数
    void setTargetVehicleSpeed(
        double value);

    void setLookAheadDistance(
        double value);

    // index == 0：DualError
    // index == 1：PurePursuit
    void setControllerModeIndex(
        int index);

    void setControllerGains(
        double headingGain,
        double lateralGain);

    // GUI 请求历史帧
    void requestReplayFrame(
        int index);

private:
    enum class ControllerMode
    {
        DualError,
        PurePursuit
    };

    // 以下几个函数都是从当前 MainWindow 中迁移过来的逻辑
    void startLateralPlan(
        double targetOffset);

    double calculatePlannedOffset(
        double x) const;

    double calculatePlannedYaw(
        double x) const;

    double normalizeAngle(
        double angle) const;

    void rebuildPlannedTrajectory();

private:
    VehicleModel vehicleModel_;

    TrajectoryController trajectoryController_;

    PurePursuitController purePursuitController_;

    LongitudinalController longitudinalController_;

    SimulationRecorder simulationRecorder_;

    TestEvaluator testEvaluator_;

    QString recordFilePath_; // 保存当前记录文件路径。

    double totalDistance_ = 0.0;

    // 当前仿真车辆最近一次 X
    double lastX_ = 0.0;

    double targetLateralOffset_ = 0.0;

    double currentLateralOffset_ = 0.0;

    double planStartX_ = 0.0;

    double planStartOffset_ = 0.0;

    double planningDistance_ = 20.0;

    bool lateralPlanActive_ = false;

    QVector<QPointF> plannedTrajectory_;

    double lookAheadDistance_ = 2.0;

    bool vehicleModelInitialized_ = false;

    bool running_ = false;

    double targetVehicleSpeed_ = 5.0;

    double currentVehicleSpeed_ = 0.0;

    double frontObstacleDistance_ = -1.0;

    bool emergencyBrakeActive_ = false;

    // 当前 DataLoader 每 100 ms 一帧
    double simulationDt_ = 0.1;

    bool replayMode_ = false;

    ControllerMode controllerMode_ =
        ControllerMode::DualError;

    QString plannerRunId_;
};

#endif