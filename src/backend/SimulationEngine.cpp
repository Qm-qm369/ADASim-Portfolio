#include "SimulationEngine.h"

#include <cmath>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include "communication/PlannerClock.h"
#include "system/LinuxLogger.h"

SimulationEngine::SimulationEngine(
    QObject *parent)
    : QObject(parent)
{
}

void SimulationEngine::configure(
    double targetSpeed,
    double planningDistance,
    double lookAheadDistance,
    double headingGain,
    double lateralGain,
    bool usePurePursuit)
{
    targetVehicleSpeed_ =
        targetSpeed;

    planningDistance_ =
        planningDistance;

    lookAheadDistance_ =
        lookAheadDistance;

    trajectoryController_.setGains(
        headingGain,
        lateralGain);

    controllerMode_ =
        usePurePursuit
            ? ControllerMode::PurePursuit
            : ControllerMode::DualError;
}

bool SimulationEngine::startRecording(
    const QString &filePath)
{
    QFileInfo info(filePath);

    QDir dir =
        info.absoluteDir(); // 获取文件所在目录 absoluteDir() 获取绝对路径目录。

    if (!dir.exists() && !dir.mkpath("."))
    {
        qCritical() << "[Recorder] Cannot create directory:" << dir.absolutePath();
        return false;
    }

    recordFilePath_ = filePath;

    bool result =
        simulationRecorder_.open(
            filePath);

    if (result)
    {
        qInfo()
            << "[Recorder] opened:"
            << filePath;
    }
    else
    {
        qWarning()
            << "[Recorder] open failed:"
            << filePath;
    }

    return result;
}

void SimulationEngine::start()
{
    if (running_)
        return;
    plannerRunId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit plannerRunChanged(plannerRunId_);

    if (!vehicleModelInitialized_)
    {
        testEvaluator_.reset();

        vehicleModel_.setState(
            0.0,
            0.0,
            0.0);

        vehicleModelInitialized_ = true;
    }

    if (replayMode_)
    {
        replayMode_ = false;

        VehicleState state = vehicleModel_.state();

        emit liveVehicleStateReady(
            state.x,
            state.y,
            state.yaw);
    }
    running_ = true;

    LinuxLogger::info(
        QString("Simulation started, target speed=%1 m/s")
            .arg(targetVehicleSpeed_, 0, 'f', 1));
}

void SimulationEngine::stop()
{
    running_ = false;

    plannerRunId_.clear();
    emit plannerRunChanged(QString());

    totalDistance_ = 0.0;

    lastX_ = 0.0;

    targetLateralOffset_ = 0.0;

    currentLateralOffset_ = 0.0;

    currentVehicleSpeed_ = 0.0;

    frontObstacleDistance_ = -1.0;

    emergencyBrakeActive_ = false;

    planStartX_ = 0.0;

    planStartOffset_ = 0.0;

    lateralPlanActive_ = false;

    plannedTrajectory_.clear();

    vehicleModel_.reset();

    vehicleModelInitialized_ = false;

    simulationRecorder_.clear();

    replayMode_ = false;

    simulationRecorder_.close();

    emit plannedTrajectoryUpdated(
        QVector<QPointF>());

    LinuxLogger::info("Simulation stopped");
}

void SimulationEngine::setTargetVehicleSpeed(
    double value)
{
    targetVehicleSpeed_ = value;
}

void SimulationEngine::setLookAheadDistance(
    double value)
{
    lookAheadDistance_ = value;
}

// 控制器模式
void SimulationEngine::setControllerModeIndex(
    int index)
{
    if (index == 0)
    {
        controllerMode_ =

            ControllerMode::DualError;
    }
    else
    {
        controllerMode_ =
            ControllerMode::PurePursuit;
    }
}

// 控制增益
void SimulationEngine::setControllerGains(
    double headingGain,
    double lateralGain)
{
    trajectoryController_.setGains(
        headingGain,
        lateralGain);
}

// 搬前方障碍物距离
void SimulationEngine::onFrontObstacleDistanceUpdated(
    double distance)
{
    frontObstacleDistance_ =
        distance;
}

double SimulationEngine::calculatePlannedOffset(
    double x) const
{
    if (!lateralPlanActive_)
    {
        return targetLateralOffset_;
    }

    double u =
        (x - planStartX_) /
        planningDistance_;

    if (u <= 0.0)
    {
        return planStartOffset_;
    }

    if (u >= 1.0)
    {
        return targetLateralOffset_;
    }

    double smooth =
        3.0 * u * u -
        2.0 * u * u * u;

    return planStartOffset_ +
           (targetLateralOffset_ -
            planStartOffset_) *
               smooth;
}

double SimulationEngine::calculatePlannedYaw(
    double x) const
{
    if (!lateralPlanActive_)
    {
        return 0.0;
    }

    const double sampleDistance =
        0.1;

    double y1 =
        calculatePlannedOffset(x);

    double y2 =
        calculatePlannedOffset(
            x + sampleDistance);

    double dx =
        sampleDistance;

    double dy =
        y2 - y1;

    return std::atan2(
        dy,
        dx);
}

double SimulationEngine::normalizeAngle(
    double angle) const
{
    while (angle > M_PI)
    {
        angle -= 2.0 * M_PI;
    }

    while (angle < -M_PI)
    {
        angle += 2.0 * M_PI;
    }

    return angle;
}

// 重新构建规划轨迹
void SimulationEngine::rebuildPlannedTrajectory()
{
    plannedTrajectory_.clear();

    const int pointCount = 40;

    for (int i = 0; i <= pointCount; ++i)
    {
        double u = static_cast<double>(i) / pointCount;

        double smooth = 3.0 * u * u - 2.0 * u * u * u;

        double x = planStartX_ + planningDistance_ * u;

        double offset = planStartOffset_ +
                        (targetLateralOffset_ -
                         planStartOffset_) *
                            smooth;

        plannedTrajectory_.append(QPointF(x, offset));
    }

    emit plannedTrajectoryUpdated(plannedTrajectory_);
}

void SimulationEngine::startLateralPlan(
    double targetOffset)
{
    lastX_ = vehicleModel_.state().x;

    planStartX_ =
        lastX_;

    planStartOffset_ =
        currentLateralOffset_;

    targetLateralOffset_ =
        targetOffset;

    lateralPlanActive_ =
        true;

    qDebug()
        << "开始横向规划:"
        << "startX =" << planStartX_
        << "startOffset =" << planStartOffset_
        << "targetOffset =" << targetLateralOffset_;

    rebuildPlannedTrajectory();
}

void SimulationEngine::onSimulationTick(
    const QVector<QPointF> &points)
{
    if (!running_ || !vehicleModelInitialized_ || replayMode_)
    {
        return;
    }
    VehicleState currentState = vehicleModel_.state();

    // ==========================================
    // 横向规划是否结束
    // ==========================================

    if (lateralPlanActive_ &&
        currentState.x >=
            planStartX_ +
                planningDistance_)
    {
        lateralPlanActive_ = false;
    }

    // ==========================================
    // 当前参考横向位置
    // ==========================================

    double targetY = calculatePlannedOffset(currentState.x);

    // ==========================================
    // 当前参考航向
    // ==========================================

    double previewX = currentState.x + lookAheadDistance_;

    double targetYaw = 0.0;

    if (lateralPlanActive_)
    {
        targetYaw =
            calculatePlannedYaw(
                previewX);
    }

    // ==========================================
    // 纵向控制
    // ==========================================

    LongitudinalControlOutput longitudinalControl =
        longitudinalController_.compute(
            targetVehicleSpeed_,
            currentVehicleSpeed_,
            frontObstacleDistance_);

    if (longitudinalControl.emergencyBrake &&
        !emergencyBrakeActive_)
    {
        LinuxLogger::warning(
            QString(
                "Emergency brake triggered, distance=%1 m, TTC=%2 s")
                .arg(
                    frontObstacleDistance_,
                    0,
                    'f',
                    1)
                .arg(
                    longitudinalControl.ttc,
                    0,
                    'f',
                    2));
    }

    if (!longitudinalControl.emergencyBrake &&
        emergencyBrakeActive_)
    {
        LinuxLogger::info(
            "Emergency brake released");
    }

    emergencyBrakeActive_ =
        longitudinalControl.emergencyBrake;

    // v = v0 + a * dt
    currentVehicleSpeed_ +=
        longitudinalControl.acceleration *
        simulationDt_;

    if (currentVehicleSpeed_ < 0.0)
    {
        currentVehicleSpeed_ = 0.0;
    }

    // ==========================================
    // 横向控制
    // ==========================================

    ControlOutput control;

    if (controllerMode_ ==
        ControllerMode::DualError)
    {
        control =
            trajectoryController_.compute(
                currentState,
                targetY,
                targetYaw,
                currentVehicleSpeed_);
    }
    else
    {
        double pursuitX =
            currentState.x +
            lookAheadDistance_;

        double pursuitY =
            calculatePlannedOffset(
                pursuitX);

        QPointF pursuitTarget(
            pursuitX,
            pursuitY);

        control.steeringAngle =
            purePursuitController_.computeSteering(
                currentState,
                pursuitTarget);

        control.lateralError =
            targetY -
            currentState.y;

        double pursuitYaw =
            std::atan2(
                pursuitY -
                    currentState.y,
                pursuitX -
                    currentState.x);

        control.headingError =
            normalizeAngle(
                pursuitYaw -
                currentState.yaw);
    }

    // ==========================================
    // VehicleModel 推进一步
    // ==========================================

    VehicleState newState =
        vehicleModel_.update(
            currentVehicleSpeed_,
            control.steeringAngle,
            simulationDt_);

    currentLateralOffset_ =
        newState.y;

    // ==========================================
    // 重新计算当前误差
    // ==========================================

    double newTargetY =
        calculatePlannedOffset(
            newState.x);

    double currentLateralError =
        newTargetY -
        newState.y;

    bool targetSettled =
        !lateralPlanActive_ &&
        std::abs(
            currentLateralError) < 0.05 &&
        std::abs(
            newState.yaw) < 0.02;

    // ==========================================
    // 行驶里程
    // ==========================================

    double dx =
        newState.x -
        currentState.x;

    double dy =
        newState.y -
        currentState.y;

    double distance =
        std::sqrt(
            dx * dx +
            dy * dy);

    totalDistance_ +=
        distance;

    double speedKmH =
        currentVehicleSpeed_ *
        3.6;

    // ==========================================
    // 交给 DataManager
    // ==========================================

    emit simulationFrameReady(
        newState.x,
        newState.y,
        newState.yaw,
        points,
        plannerRunId_,
        plannerNowMs());

    // ==========================================
    // 保存 SimulationFrame
    // ==========================================

    SimulationFrame frame;

    frame.vehicle =
        newState;

    frame.targetY =
        newTargetY;

    frame.targetYaw =
        targetYaw;

    frame.lateralError =
        currentLateralError;

    frame.headingError =
        control.headingError;

    frame.steeringAngle =
        control.steeringAngle;

    frame.speedKmH =
        speedKmH;

    frame.totalDistance =
        totalDistance_;

    frame.targetSpeedKmH =
        longitudinalControl.safeTargetSpeed *
        3.6;

    frame.acceleration =
        longitudinalControl.acceleration;

    frame.frontObstacleDistance =
        frontObstacleDistance_;

    frame.ttc =
        longitudinalControl.ttc;

    frame.emergencyBrake =
        longitudinalControl.emergencyBrake;

    frame.controllerMode =
        controllerMode_ ==
                ControllerMode::DualError
            ? 0
            : 1;

    simulationRecorder_.append(frame);

    testEvaluator_.processFrame(
        frame);

    int currentIndex =
        simulationRecorder_.size() -
        1;

    // GUI 数据一次发出去
    emit frameUpdated(
        frame,
        currentIndex,
        targetSettled);

    lastX_ =
        newState.x;
}

void SimulationEngine::requestReplayFrame(
    int index)
{
    SimulationFrame frame;

    if (!simulationRecorder_.frameAt(
            index,
            frame))
    {
        return;
    }

    replayMode_ = true;

    emit replayFrameReady(
        index,
        simulationRecorder_.size() - 1,
        frame);
}

TestResult SimulationEngine::testResult(
    AebExpectation expectation) const
{
    return testEvaluator_.result(expectation);
}

void SimulationEngine::pause()
{
    running_ = false;
    plannerRunId_.clear();
    emit plannerRunChanged(QString());
}

void SimulationEngine::onLateralControlReceived(double offset)
{
    planStartOffset_ = offset;
}