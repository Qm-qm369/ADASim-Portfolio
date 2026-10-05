#include "LongitudinalController.h"

LongitudinalController::LongitudinalController()
{
}

LongitudinalControlOutput LongitudinalController::compute(
    double desiredSpeed,
    double currentSpeed,
    double frontObstacleDistance) const
{
    LongitudinalControlOutput output;

    // 默认没有危险：
    // 安全目标速度就是用户设置的目标速度
    output.safeTargetSpeed = desiredSpeed;

    // =====================================
    // 1. 计算TTC
    // =====================================

    if (frontObstacleDistance > 0.0 && currentSpeed > 0.1)
    {
        output.ttc = frontObstacleDistance / currentSpeed;
    }

    // =====================================
    // 2. 判断是否需要紧急制动
    // =====================================

    bool distanceDanger = frontObstacleDistance > 0.0 && frontObstacleDistance <= emergencyDistance_;

    bool ttcDanger = output.ttc > 0.0 && output.ttc <= emergencyTtc_;

    // 只要两个满足一个 就进入紧急制动
    if (distanceDanger || ttcDanger)
    {
        output.safeTargetSpeed = 0.0;

        output.acceleration = emergencyDeceleration_;

        output.emergencyBrake = true;

        return output;
    }

    // =====================================
    // 3. 进入减速区域
    // =====================================

    if (frontObstacleDistance > 0.0 &&
        frontObstacleDistance < slowDistance_)
    {
        double ratio =
            (frontObstacleDistance - stopDistance_) /
            (slowDistance_ - stopDistance_); // 根据距离计算安全速度

        if (ratio < 0.0)
        {
            ratio = 0.0;
        }

        if (ratio > 1.0)
        {
            ratio = 1.0;
        }

        output.safeTargetSpeed =
            desiredSpeed * ratio;
    }

    // =====================================
    // 4. 速度误差 -> 加速度 误差越大 加速度越大
    // =====================================

    double speedError =
        output.safeTargetSpeed -
        currentSpeed;

    output.acceleration =
        speedKp_ * speedError;

    // =====================================
    // 5. 加速度限幅
    // =====================================

    if (output.acceleration >
        maxAcceleration_)
    {
        output.acceleration =
            maxAcceleration_;
    }

    if (output.acceleration <
        maxDeceleration_)
    {
        output.acceleration =
            maxDeceleration_;
    }

    return output;
}