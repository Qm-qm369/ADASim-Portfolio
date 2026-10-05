// 纵向控制器
// 根据目标速度、当前速度、前方障碍物距离，计算车辆应该保持多少速度，以及需要加速还是减速。
// 开环：没有反馈，只管输出，不管结果；闭环：拿实际结果回来对比，不断修正输出。

#ifndef LONGITUDINALCONTROLLER_H
#define LONGITUDINALCONTROLLER_H

struct LongitudinalControlOutput
{
    // 考虑安全距离以后，当前真正允许达到的目标速度
    double safeTargetSpeed = 0.0;

    // 本帧加速度
    // > 0 加速
    // < 0 减速
    double acceleration = 0.0;

    // Time To Collision 碰撞时间 / 预计碰撞时间
    // -1 表示当前无法计算
    double ttc = -1.0;

    // 是否触发紧急制动
    bool emergencyBrake = false;
};

class LongitudinalController
{
public:
    LongitudinalController();

    LongitudinalControlOutput compute(
        double desiredSpeed,
        double currentSpeed,
        double frontObstacleDistance) const;

private:
    // 速度P控制增益
    double speedKp_ = 1.2;

    // 最大正常加速度
    double maxAcceleration_ = 2.0;

    // 最大正常减速度
    double maxDeceleration_ = -4.0;

    // 紧急制动减速度
    double emergencyDeceleration_ = -7.0;

    // 进入这个距离开始主动降速
    double slowDistance_ = 20.0;

    // 接近这个距离时目标速度降到0
    double stopDistance_ = 5.0;

    // 小于这个距离直接认为危险
    double emergencyDistance_ = 3.0;

    // TTC低于1秒进入紧急制动
    double emergencyTtc_ = 1.0;
};

#endif