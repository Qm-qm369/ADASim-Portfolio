#ifndef PUREPURSUITCONTROLLER_H
#define PUREPURSUITCONTROLLER_H

#include <QPointF>

#include "VehicleModel.h"

class PurePursuitController
{
public:
    PurePursuitController();

    // 根据车辆现在在哪里+前往目标点在哪里 输出steeringAngle 我要打多少方向，才能沿一个圆弧追到前面的目标点？
    double computeSteering(
        const VehicleState &state,
        const QPointF &targetPoint) const;

private:
    double wheelBase_ = 2.7;
    double maxSteeringAngle_ = 0.436332;
};

#endif