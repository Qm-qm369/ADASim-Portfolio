#ifndef VIEW2D_H
#define VIEW2D_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QPolygonF>

class QPainter;
class QMouseEvent;

class QPainter;

/**
 * @brief ADASim 二维俯视图
 */
class View2D : public QWidget
{
    Q_OBJECT

public:
    explicit View2D(QWidget *parent = nullptr);
    ~View2D();

public slots:
    // 接收 PathPredictor 计算出的未来轨迹
    void updatePredictedPath(
        const QVector<QPointF> &path);

    void updateObstacles(
        const QVector<QPointF> &obstacles);

    // 接收新的车辆位姿
    void updateVehiclePosition(double x,
                               double y,
                               double yaw);
    void drawTrajectory(QPainter &painter);

    void updatePlannedTrajectory(const QVector<QPointF> &trajectory);
    // 更新车道跟踪 / 目标跟踪模块的调试信息，在界面上绘图展示。
    void updateTrackingDebug(
        const QPointF &targetPoint,
        double lateralError,
        double steeringAngle);

    void clearTrackingDebug();

    // 只是把屏幕切到历史帧看看不记录新轨迹
    void showReplayFrame(double x, double y, double yaw);

signals:
    void userObstacleAdded(
        double worldX,
        double worldY);

protected:
    // QWidget 需要重绘时，Qt 会自动调用
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(
        QMouseEvent *event) override;

private:
    void drawObstacles(
        QPainter &painter);

    // 绘制背景网格
    void drawMap(QPainter &painter);

    // 绘制自车
    void drawVehicle(QPainter &painter);

    // 绘制未来预测轨迹
    void drawPredictedPath(QPainter &painter);

    // 绘制Lattice候选轨迹
    void drawPlanning(QPainter &painter);

    void drawTrackingDebug(QPainter &painter);

private:
    // PathPredictor预测出的未来世界坐标
    QVector<QPointF> predictedPath_;
    // 自车过去走过的轨迹
    QPolygonF trajectory_;

    // 感知算法检测出来的障碍物
    QVector<QPointF> obstacles_;

    // 用户右键放置的真实障碍物
    QVector<QPointF> globalUserObstacles_;

    // 像素/米，用于把物理尺寸转换成屏幕尺寸
    double zoom_ = 20.0;

    // 当前车辆全局位姿
    double vehicleX_ = 0.0;
    double vehicleY_ = 0.0;
    double vehicleYaw_ = 0.0;

    QVector<QPointF> plannedTrajectory_;

    QPointF trackingTarget_;

    double trackingLateralError_ = 0.0;
    double steeringAngle_ = 0.0;

    bool trackingDebugActive_ = false;
};

#endif // VIEW2D_H