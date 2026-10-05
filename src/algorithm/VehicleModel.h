/*
    根据方向盘算车辆运动
*/

#ifndef VEHICLEMODEL_H
#define VEHICLEMODEL_H

struct VehicleState
{
    double x = 0.0;
    double y = 0.0;
    double yaw = 0.0;
};

class VehicleModel
{
public:
    VehicleModel();

    // 设置车辆当前状态
    void setState(double x, double y, double yaw);

    // 重置车辆
    void reset();

    // 根据速度和转向角计算下一帧车辆状态
    VehicleState update(double speed, double steeringAngle, double dt);

    // 获取当前车辆状态
    VehicleState state() const; // 末尾const 承诺这个成员函数不会修改当前对象的成员变量。

private:
    VehicleState state_;

    // 简化车辆轴距，V1.4学习版参数
    double wheelBase_ = 2.7;
};

#endif