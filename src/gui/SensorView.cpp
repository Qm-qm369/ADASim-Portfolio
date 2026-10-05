#include "SensorView.h"

#include <QPainter>
#include <QFont>

SensorView::SensorView(
    QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(
        300,
        300);

    // paintEvent会自己画满背景
    setAttribute(
        Qt::WA_OpaquePaintEvent);

    setAttribute(
        Qt::WA_NoSystemBackground);
}

SensorView::~SensorView()
{
}

void SensorView::updatePointCloud(
    const QVector<QPointF> &points)
{
    pointCloud_ = points;

    update();
}

void SensorView::paintEvent(
    QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.setRenderHint(
        QPainter::Antialiasing);

    // 把当前这个控件的整个画面，刷上一层暗黑色背景
    painter.fillRect(
        rect(),
        QColor(5, 5, 8));

    drawLidarView(painter);

    painter.setPen(
        QColor(0, 255, 136));

    painter.setFont(
        QFont(
            "Arial",
            10,
            QFont::Bold));

    painter.drawText(
        15,
        25,
        "LiDAR POINT CLOUD");
}

void SensorView::drawLidarView(
    QPainter &painter)
{
    const int centerX =
        width() / 2;

    const int centerY =
        height() / 2;

    const int maxRadius =
        qMin(width(), height()) / 2 - 20;

    // =====================================
    // 1. 雷达圆环
    // =====================================

    painter.setPen(
        QPen(
            QColor(0, 255, 136, 40),
            1));

    for (int r = maxRadius / 4;
         r <= maxRadius;
         r += maxRadius / 4)
    {
        painter.drawEllipse(
            QPoint(centerX, centerY),
            r,
            r);
    }

    // 十字线
    painter.drawLine(
        centerX,
        centerY - maxRadius,
        centerX,
        centerY + maxRadius);

    painter.drawLine(
        centerX - maxRadius,
        centerY,
        centerX + maxRadius,
        centerY);

    // 50米映射到雷达最大半径
    const float scale =
        maxRadius / 50.0f;

    const float redDistSq =
        15.0f * 15.0f;

    const float yellowDistSq =
        30.0f * 30.0f;

    const float maxDistSq =
        50.0f * 50.0f;

    QVector<QPointF> redPoints;

    QVector<QPointF> yellowPoints;

    QVector<QPointF> greenPoints;

    // =====================================
    // 2. 点云坐标转换与分类
    // =====================================

    for (const QPointF &point : pointCloud_)
    {
        float distSq =
            point.x() * point.x() +
            point.y() * point.y();

        if (distSq > maxDistSq)
        {
            continue;
        }

        QPointF screenPoint(
            centerX + point.x() * scale,

            centerY - point.y() * scale);

        if (distSq < redDistSq)
        {
            redPoints.append(
                screenPoint);
        }
        else if (distSq < yellowDistSq)
        {
            yellowPoints.append(
                screenPoint);
        }
        else
        {
            greenPoints.append(
                screenPoint);
        }
    }

    // =====================================
    // 3. 批量绘制
    // =====================================

    painter.setPen(
        QPen(
            QColor(0, 255, 136),
            2));

    painter.drawPoints(
        greenPoints.data(),
        greenPoints.size());

    painter.setPen(
        QPen(
            QColor(255, 200, 50),
            3));

    painter.drawPoints(
        yellowPoints.data(),
        yellowPoints.size());

    painter.setPen(
        QPen(
            QColor(255, 50, 50),
            4));

    painter.drawPoints(
        redPoints.data(),
        redPoints.size());

    // 自车位于雷达中心
    painter.setPen(Qt::NoPen);

    painter.setBrush(Qt::white);

    painter.drawEllipse(
        QPoint(centerX, centerY),
        3,
        3);
}