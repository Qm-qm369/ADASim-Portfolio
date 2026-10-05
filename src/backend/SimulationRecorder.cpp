#include "SimulationRecorder.h"

SimulationRecorder::SimulationRecorder()
{
}

bool SimulationRecorder::open(
    const QString &fileName)
{

    file_.setFileName(fileName);

    if (!file_.open(
            QIODevice::WriteOnly |
            QIODevice::Text))
    {
        return false;
    }

    stream_.setDevice(&file_); // 让文本流绑定到这个文件。

    stream_
        << "x,"
        << "y,"
        << "yaw,"
        << "speed,"
        << "ttc,"
        << "aeb\n";

    recording_ = true;

    return true;
}

void SimulationRecorder::close()
{

    if (!recording_)
    {
        return;
    }

    stream_.flush();

    file_.close();

    recording_ = false;
}

const QVector<SimulationFrame> &
SimulationRecorder::frames() const
{
    return frames_;
}

void SimulationRecorder::clear()
{
    frames_.clear();
}

void SimulationRecorder::append(const SimulationFrame &frame)
{
    frames_.append(frame);

    if (recording_)
    {

        stream_
            << frame.vehicle.x
            << ","
            << frame.vehicle.y
            << ","
            << frame.vehicle.yaw
            << ","
            << frame.speedKmH
            << ","
            << frame.ttc
            << ","
            << frame.emergencyBrake
            << "\n";
    }

    if (frames_.size() > maxFrames_)
    {
        frames_.pop_front();
    }
}

int SimulationRecorder::size() const
{
    return frames_.size();
}

bool SimulationRecorder::frameAt(int index, SimulationFrame &frame) const
{
    if (index < 0 || index >= frames_.size())
    {
        return false;
    }

    frame = frames_[index];
    return true;
}