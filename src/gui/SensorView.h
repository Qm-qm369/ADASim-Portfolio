#ifndef SENSORVIEW_H
#define SENSORVIEW_H

#include <QWidget>
#include <QVector>
#include <QPointF>

class QPainter;

class SensorView : public QWidget
{
    Q_OBJECT

public:
    explicit SensorView(
        QWidget *parent = nullptr);

    ~SensorView();

public slots:

    void updatePointCloud(
        const QVector<QPointF> &points);

protected:
    void paintEvent(
        QPaintEvent *event) override;

private:
    void drawLidarView(
        QPainter &painter);

private:
    QVector<QPointF> pointCloud_;
};

#endif