#ifndef SIMULATIONRECORDER_H
#define SIMULATIONRECORDER_H

#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QString>

#include "algorithm/VehicleModel.h"

struct SimulationFrame
{
    VehicleState vehicle;

    double targetY = 0.0;
    double targetYaw = 0.0;

    double lateralError = 0.0;
    double headingError = 0.0;

    double steeringAngle = 0.0;

    double speedKmH = 0.0;
    double totalDistance = 0.0;

    int controllerMode = 0;

    // V1.8纵向控制
    double targetSpeedKmH = 0.0;
    double acceleration = 0.0;

    double frontObstacleDistance = -1.0;

    double ttc = -1.0;

    bool emergencyBrake = false;
};

class SimulationRecorder
{
public:
    SimulationRecorder();

    bool open(const QString &fileName);
    void close();

    void clear();

    void append(const SimulationFrame &frame);

    int size() const;

    bool frameAt(int index, SimulationFrame &frame) const;

    const QVector<SimulationFrame> &frames() const;

private:
    QVector<SimulationFrame> frames_;

    QFile file_; // QFile类（Qt 文件操作类，继承自 QIODevice）代表磁盘上的一个文件，负责文件底层操作：打开、关闭、判断是否存在、删除、读写原始字节。

    QTextStream stream_; // 包装一个 QIODevice（比如 QFile），专门用来读写文本，自动处理字符编码、换行，支持<<和>>流式读写，类似 C++ 的std::ifstream / std::ofstream。

    bool recording_ = false;

    int maxFrames_ = 300;
};

#endif