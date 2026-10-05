#include "TrajectoryController.h"

TrajectoryController::TrajectoryController()
{
}

double TrajectoryController::normalizeAngle(double angle) const
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

/*
    > 1. 横向误差 lateralError：车子离参考线左右偏了多少（位置误差）
    > 2. 航向误差 headingError：车头朝向和参考方向差多少（角度误差）
*/
ControlOutput TrajectoryController::compute(const VehicleState &state, double targetY, double targetYaw, double speed) const
{
    ControlOutput output;

    // 车辆距离参考轨迹横向差多少
    output.lateralError = targetY - state.y;

    // 车辆车头和参考轨迹方向差多少
    output.headingError = normalizeAngle(targetYaw - state.yaw);

    double safeSpeed = std::abs(speed);

    if (safeSpeed < 0.1)
    {
        safeSpeed = 0.1;
    }

    // 横向位置误差产生的转向修正
    double lateralTerm = std::atan2(kLateral_ * output.lateralError, safeSpeed); // 反正切函数，返回弧度值

    // 航向误差 + 横向误差
    output.steeringAngle = kHeading_ * output.headingError + lateralTerm;

    // 限制最大转向角
    if (output.steeringAngle > maxSteeringAngle_)
    {
        output.steeringAngle = maxSteeringAngle_;
    }

    if (output.steeringAngle < -maxSteeringAngle_)
    {
        output.steeringAngle = -maxSteeringAngle_;
    }

    return output;
}

void TrajectoryController::setGains(
    double headingGain,
    double lateralGain)
{
    kHeading_ = headingGain;
    kLateral_ = lateralGain;
}