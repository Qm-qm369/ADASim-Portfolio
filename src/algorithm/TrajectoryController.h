/*
    根据误差算方向盘
*/

#ifndef TRAJECTORYCONTROLLER_H
#define TRAJECTORYCONTROLLER_H

#include "VehicleModel.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 专门保存这一帧控制器算出来的结果
struct ControlOutput
{
    double steeringAngle = 0.0; // 最终前轮转角
    double lateralError = 0.0;  // 车辆横向位置差多少
    double headingError = 0.0;  // 车头方向差多少
};

class TrajectoryController
{
public:
    TrajectoryController();

    ControlOutput compute(const VehicleState &state, double targetY, double targetYaw, double speed) const;
    void setGains(double headingGain, double lateralGain);

private:
    double normalizeAngle(double angle) const;

    // 航向误差权重
    double kHeading_ = 1.0;

    // 横向误差权重
    double kLateral_ = 1.5;

    // 最大转向角约25°
    double maxSteeringAngle_ = 0.436332;
};

#endif