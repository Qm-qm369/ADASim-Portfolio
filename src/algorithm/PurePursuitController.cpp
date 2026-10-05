#include "PurePursuitController.h"

#include <cmath>

// ----------------------------------------------
// 已知车辆现在在哪里、车头朝哪里，再给它一个前方目标点，Pure Pursuit 算法算前轮应该转多少角度，才能朝这个目标点弯过。
// ----------------------------------------------

PurePursuitController::PurePursuitController()
{
}

double PurePursuitController::computeSteering(
    const VehicleState &state,
    const QPointF &targetPoint) const
{
    // 世界坐标下目标点相对车辆的位置
    double dx = targetPoint.x() - state.x;
    double dy = targetPoint.y() - state.y;

    double cosYaw = std::cos(state.yaw);
    double sinYaw = std::sin(state.yaw);

    // 世界坐标 -> 车辆局部坐标
    double localX = dx * cosYaw + dy * sinYaw;
    double localY = -dx * sinYaw + dy * cosYaw;

    // 车辆到目标点的真实距离
    double lookAheadDistance =
        std::sqrt(localX * localX + localY * localY);

    if (lookAheadDistance < 0.1)
    {
        return 0.0;
    }

    // 根据目标点位置计算需要的轨迹⭐️曲率 这条圆弧应该弯多厉害
    // 曲率计算公式 k = 2y/L^2
    double curvature =
        2.0 * localY /
        (lookAheadDistance * lookAheadDistance);

    // 曲率 -> 前轮转角
    double steeringAngle =
        std::atan(wheelBase_ * curvature);

    if (steeringAngle > maxSteeringAngle_)
    {
        steeringAngle = maxSteeringAngle_;
    }

    if (steeringAngle < -maxSteeringAngle_)
    {
        steeringAngle = -maxSteeringAngle_;
    }

    return steeringAngle;
}