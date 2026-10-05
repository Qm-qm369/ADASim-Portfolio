#include "ControlMonitor.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ControlMonitor::ControlMonitor(QWidget *parent) // 本身是一个 Qt 控件
    : QWidget(parent)
{
    setMinimumHeight(180);
}

void ControlMonitor::appendSample(double lateralError, double steeringAngle)
{
    lateralErrors_.append(lateralError);

    steeringDegrees_.append(steeringAngle * 180.0 / M_PI);

    while (lateralErrors_.size() > maxSamples_)
    {
        lateralErrors_.pop_front();
    }

    while (steeringDegrees_.size() > maxSamples_)
    {
        steeringDegrees_.pop_front();
    }

    update();
}

void ControlMonitor::clear()
{
    lateralErrors_.clear();
    steeringDegrees_.clear();

    update();
}

/*
真正“把一组数字画成曲线”的函数

painter → 用谁画
values → 要画的一组数据
rect → 在屏幕哪个矩形区域画
range → Y轴允许显示的范围
*/
void ControlMonitor::drawSeries(
    QPainter &painter,
    const QVector<double> &values,
    const QRectF &rect,
    double range)
{
    if (values.size() < 2)
    {
        return;
    }

    QPainterPath path;

    for (int i = 0; i < values.size(); ++i)
    {
        double ratio = static_cast<double>(i) / (values.size() - 1);

        double x = rect.left() + ratio * rect.width();

        double value = std::max(-range, std::min(range, values[i]));

        double y = rect.center().y() - value / range * rect.height() / 2.0;

        if (i == 0)
        {
            path.moveTo(x, y);
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    painter.drawPath(path);
}

void ControlMonitor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.setRenderHint(
        QPainter::Antialiasing);

    painter.fillRect(
        rect(),
        QColor(10, 15, 25));

    painter.setPen(Qt::white);

    painter.drawText(
        10,
        20,
        "控制状态");

    QRectF errorRect(
        40,
        30,
        width() - 50,
        55);

    QRectF steeringRect(
        40,
        105,
        width() - 50,
        55);

    painter.setPen(
        QPen(QColor(80, 90, 100), 1));

    painter.drawLine(
        errorRect.left(),
        errorRect.center().y(),
        errorRect.right(),
        errorRect.center().y());

    painter.drawLine(
        steeringRect.left(),
        steeringRect.center().y(),
        steeringRect.right(),
        steeringRect.center().y());

    painter.setPen(
        QPen(QColor(255, 210, 0), 2));

    drawSeries(
        painter,
        lateralErrors_,
        errorRect,
        1.0);

    painter.setPen(
        QPen(QColor(0, 200, 255), 2));

    drawSeries(
        painter,
        steeringDegrees_,
        steeringRect,
        25.0);

    painter.setPen(Qt::white);

    painter.drawText(
        5,
        static_cast<int>(
            errorRect.center().y()),
        "eY");

    painter.drawText(
        5,
        static_cast<int>(
            steeringRect.center().y()),
        "steer");
}