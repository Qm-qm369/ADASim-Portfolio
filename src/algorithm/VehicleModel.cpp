#include "VehicleModel.h"

#include <cmath>

VehicleModel::VehicleModel()
{
}

void VehicleModel::setState(double x, double y, double yaw)
{
    state_.x = x;
    state_.y = y;
    state_.yaw = yaw;
}

void VehicleModel::reset()
{
    state_.x = 0.0;
    state_.y = 0.0;
    state_.yaw = 0.0;
}

VehicleState VehicleModel::state() const
{
    return state_;
}

VehicleState VehicleModel::update(double speed, double steeringAngle, double dt)
{
    // 当前车头方向决定车辆这一帧往哪里走
    state_.x += speed * std::cos(state_.yaw) * dt;
    state_.y += speed * std::sin(state_.yaw) * dt;

    // 转向角决定车辆航向角如何变化
    state_.yaw += speed / wheelBase_ * std::tan(steeringAngle) * dt;

    return state_;
}