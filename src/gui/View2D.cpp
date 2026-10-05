#include "View2D.h"

#include <QPainter>
#include <QPen>
#include <QLinearGradient>
#include <cmath>
#include <QMouseEvent>
#include <QPainterPath> // 用来描述一条“绘图路径”

View2D::View2D(QWidget *parent)
    : QWidget(parent)
{
    // 防止视图被压得太小
    setMinimumSize(400, 400);
}

View2D::~View2D()
{
}

/**
 * paintEvent是QWidget的虚事件函数 属于Qt的事件系统
 * update()并不是直接调用：paintEvent(); 向Qt发出：需要重绘的请求。
 * Qt 需要刷新 View2D 时，会自动调用 paintEvent()
 */
void View2D::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.setRenderHint(
        QPainter::Antialiasing);

    painter.fillRect(
        rect(),
        QColor(5, 8, 17));

    // 背景
    drawMap(painter);

    // V0.9 历史轨迹
    drawTrajectory(painter);

    // V0.9 未来预测轨迹
    drawPredictedPath(painter);

    // V0.10规划候选轨迹
    drawPlanning(painter);

    drawTrackingDebug(painter);

    // 障碍物
    drawObstacles(painter);

    // 自车
    drawVehicle(painter);
}

/**
 * 绘制背景网格
 */
void View2D::drawMap(QPainter &painter)
{
    // QPen画笔：专门控制线条的颜色、粗细、样式。
    // A=50：Alpha 透明度，0 完全透明，255 完全不透明。50 代表线条半透明，淡淡的网格，不会抢主体画面
    QPen gridPen(QColor(0, 255, 136, 50));
    gridPen.setWidth(1); // 网格线条宽度 1 像素。

    // 把配置好的笔交给 painter，后续所有drawLine都使用这个半透绿色画笔。
    painter.setPen(gridPen);

    const int gridSize = 50;

    // 根据车辆位置计算网格偏移
    int offsetX = static_cast<int>(vehicleX_ * zoom_) % gridSize;

    int offsetY = static_cast<int>(vehicleY_ * zoom_) % gridSize;

    // 绘制竖线
    for (int x = -offsetX; x < width(); x += gridSize)
    {
        painter.drawLine(x, 0, x, height());
    }

    // 绘制横线
    for (int y = offsetY; y < height(); y += gridSize)
    {
        painter.drawLine(0, y, width(), y);
    }
}

/**
 * 绘制自车
 */
void View2D::drawVehicle(QPainter &painter)
{
    // 保存当前 QPainter 状态
    // restore()：恢复到 save 之前状态。
    painter.save();

    // 与原项目一致：自车放在画面的 1/4 位置
    int screenX = width() / 4;
    int screenY = height() / 2;

    // 把坐标原点移动到汽车中心
    painter.translate(screenX, screenY);

    // 根据航向角旋转汽车
    painter.rotate(-vehicleYaw_ * 180.0 / M_PI);

    // 原源码使用的车辆物理尺寸
    const double physicalLength = 4.6;
    const double physicalWidth = 2.2;

    // 米 -> 像素
    int carLength =
        static_cast<int>(physicalLength * zoom_);

    int carWidth =
        static_cast<int>(physicalWidth * zoom_);

    // =========================
    // 1. 汽车主体
    // =========================

    QLinearGradient carGradient(
        -carLength / 2,
        0,
        carLength / 2,
        0);

    carGradient.setColorAt(0, QColor(220, 220, 230));
    carGradient.setColorAt(1, QColor(160, 170, 180));

    // pen：图形的边框线条- brush：图形内部填充
    painter.setBrush(carGradient);
    painter.setPen(QPen(Qt::white, 1));

    painter.drawRoundedRect(
        -carLength / 2,
        -carWidth / 2,
        carLength,
        carWidth,
        4,
        4);

    // =========================
    // 2. 车头标识
    // =========================
    //
    // 用深色区域表示前挡风玻璃，
    // 这样一眼就能判断车头朝向。

    painter.setBrush(QColor(20, 25, 30));
    painter.setPen(Qt::NoPen);

    painter.drawRect(
        static_cast<int>(carLength * 0.1),
        static_cast<int>(-carWidth * 0.4),
        static_cast<int>(carLength * 0.15),
        static_cast<int>(carWidth * 0.8));

    // 恢复之前的 QPainter 状态
    // 恢复之前的坐标原点等
    painter.restore();
}

void View2D::drawObstacles(
    QPainter &painter)
{
    painter.setPen(Qt::NoPen);

    // =====================================
    // 1. 用户手动添加的真值障碍物
    // =====================================

    painter.setBrush(
        QColor(100, 110, 120, 180));

    // 你用鼠标右键手动添加到沙盒里的障碍物。
    for (const QPointF &obstacle : globalUserObstacles_)
    {
        int x =
            width() / 4 +
            (obstacle.x() - vehicleX_) * zoom_;

        int y =
            height() / 2 -
            (obstacle.y() - vehicleY_) * zoom_;

        // 画一个椭圆
        painter.drawEllipse(
            QPoint(x, y),
            6,
            6);
    }

    // =====================================
    // 2. 感知算法检测结果 感知算法检测以后，交给 View2D 显示的障碍物位置。
    // =====================================

    for (const QPointF &obstacle : obstacles_)
    {
        int x =
            width() / 4 +
            (obstacle.x() - vehicleX_) * zoom_;

        int y =
            height() / 2 -
            (obstacle.y() - vehicleY_) * zoom_;

        // 检测范围光晕
        painter.setBrush(
            QColor(255, 50, 50, 100));

        painter.drawEllipse(
            QPoint(x, y),
            10,
            10);

        // 障碍物中心
        painter.setBrush(
            QColor(255, 50, 50));

        painter.drawEllipse(
            QPoint(x, y),
            4,
            4);
    }
}

// 它不是识别你点了哪个物体，而是直接通过event->pos()读取你鼠标点击的像素坐标
// 再根据zoom_和车辆位置把这个像素坐标换算成世界坐标。
void View2D::mousePressEvent(
    QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        // 屏幕像素坐标 -> 世界物理坐标
        double worldX =
            vehicleX_ +
            (event->pos().x() - width() / 4.0) / zoom_; // event->pos() Qt 传进来的鼠标事件对象。

        double worldY =
            vehicleY_ -
            (event->pos().y() - height() / 2.0) / zoom_;

        // View2D自己保存一份真值用于显示
        globalUserObstacles_.append(
            QPointF(worldX, worldY));

        // 通知DataManager
        emit userObstacleAdded(worldX, worldY);

        update();
    }

    QWidget::mousePressEvent(event);
}

void View2D::updateVehiclePosition(
    double x,
    double y,
    double yaw)
{
    /*
     * 如果时间轴突然跳转，
     * 防止历史轨迹从旧位置直接连到新位置。
     */
    if (!trajectory_.isEmpty())
    {
        QPointF last =
            trajectory_.last();

        // 如果新位置和上一位置，在 X 或 Y 任意一个方向突然相差超过 5，就认为发生了位置跳转，把以前画的轨迹清掉。
        if (std::abs(last.x() - x) > 5.0 ||
            std::abs(last.y() - y) > 5.0)
        {
            trajectory_.clear();
        }
    }

    // 更新车辆位置
    vehicleX_ = x;
    vehicleY_ = y;
    vehicleYaw_ = yaw;

    // 保存历史轨迹
    trajectory_.append(
        QPointF(x, y));

    // View2D只保留最近99个显示点
    while (trajectory_.size() > 99)
    {
        trajectory_.pop_front();
    }

    update();
}

void View2D::updateObstacles(
    const QVector<QPointF> &obstacles)
{
    obstacles_ = obstacles;

    update();
}

// 把 trajectory_ 里保存的历史轨迹点，一段一段画到 View2D 上。
void View2D::drawTrajectory(
    QPainter &painter)
{
    if (trajectory_.size() < 2)
    {
        return;
    }

    for (int i = 1; i < trajectory_.size(); ++i)
    {
        QPointF p1 = trajectory_[i - 1];

        QPointF p2 = trajectory_[i];

        // 世界坐标 -> 屏幕坐标
        int x1 = width() / 4 + (p1.x() - vehicleX_) * zoom_;

        int y1 = height() / 2 - (p1.y() - vehicleY_) * zoom_;

        int x2 = width() / 4 + (p2.x() - vehicleX_) * zoom_;

        int y2 = height() / 2 - (p2.y() - vehicleY_) * zoom_;

        // 越新的轨迹越明显
        int alpha = static_cast<int>(255.0 * i / trajectory_.size());

        painter.setPen(QPen(QColor(0, 200, 255, alpha), 2));

        painter.drawLine(x1, y1, x2, y2);
    }
}

void View2D::updatePredictedPath(
    const QVector<QPointF> &path)
{
    // 保存最新预测结果
    predictedPath_ = path;

    // 请求Qt重新绘制
    update();
}

void View2D::drawPredictedPath(
    QPainter &painter)
{
    // 没有预测结果就不画
    if (predictedPath_.isEmpty())
    {
        return;
    }

    painter.save();

    // 黄色虚线表示未来预测
    QPen predictedPen(
        QColor(255, 200, 50, 220),
        2,
        Qt::DashLine);

    painter.setPen(predictedPen);

    painter.setBrush(
        QColor(255, 200, 50, 220));

    // 第一段从当前车辆位置开始
    QPointF previousWorld(
        vehicleX_,
        vehicleY_);

    for (const QPointF &worldPoint : predictedPath_)
    {
        // ==============================
        // 上一个世界坐标 -> 屏幕坐标
        // ==============================

        QPointF previousScreen(
            width() / 4.0 + (previousWorld.x() - vehicleX_) * zoom_,

            height() / 2.0 - (previousWorld.y() - vehicleY_) * zoom_);

        // ==============================
        // 当前预测世界坐标 -> 屏幕坐标
        // ==============================

        QPointF currentScreen(
            width() / 4.0 + (worldPoint.x() - vehicleX_) * zoom_,

            height() / 2.0 - (worldPoint.y() - vehicleY_) * zoom_);

        // 连接预测轨迹
        painter.drawLine(
            previousScreen,
            currentScreen);

        // 每一个预测位置都画一个小点
        painter.drawEllipse(
            currentScreen,
            2.5,
            2.5);

        previousWorld =
            worldPoint;
    }

    painter.restore();
}

void View2D::drawPlanning(QPainter &painter)
{
    if (plannedTrajectory_.size() < 2)
    {
        return;
    }

    painter.save();
    painter.setPen(QPen(QColor(0, 255, 136, 230), 3));

    for (int i = 1; i < plannedTrajectory_.size(); ++i)
    {
        const QPointF &p1 = plannedTrajectory_[i - 1];
        const QPointF &p2 = plannedTrajectory_[i];

        QPointF s1(width() / 4.0 + (p1.x() - vehicleX_) * zoom_,
                   height() / 2.0 - (p1.y() - vehicleY_) * zoom_);

        QPointF s2(width() / 4.0 + (p2.x() - vehicleX_) * zoom_,
                   height() / 2.0 - (p2.y() - vehicleY_) * zoom_);

        painter.drawLine(s1, s2);
    }

    painter.restore();
}

void View2D::updatePlannedTrajectory(const QVector<QPointF> &trajectory)
{
    plannedTrajectory_ = trajectory;
    update();
}

void View2D::updateTrackingDebug(
    const QPointF &targetPoint,
    double lateralError,
    double steeringAngle)
{
    trackingTarget_ = targetPoint;
    trackingLateralError_ = lateralError;
    steeringAngle_ = steeringAngle;
    trackingDebugActive_ = true;

    update();
}

void View2D::clearTrackingDebug()
{
    trackingDebugActive_ = false;

    update();
}

void View2D::drawTrackingDebug(QPainter &painter)
{
    if (!trackingDebugActive_)
    {
        return;
    }

    painter.save();

    QPointF vehicleScreen(
        width() / 4.0,
        height() / 2.0);

    QPointF targetScreen(
        width() / 4.0 +
            (trackingTarget_.x() - vehicleX_) * zoom_,

        height() / 2.0 -
            (trackingTarget_.y() - vehicleY_) * zoom_);

    // 车辆到参考轨迹的误差线
    painter.setPen(
        QPen(
            QColor(255, 210, 0, 220),
            2,
            Qt::DashLine));

    painter.drawLine(
        vehicleScreen,
        targetScreen);

    // 参考目标点
    painter.setBrush(
        QColor(255, 210, 0));

    painter.drawEllipse(
        targetScreen,
        5,
        5);

    // 控制信息
    painter.setPen(Qt::white);

    double steeringDegree =
        steeringAngle_ * 180.0 / M_PI;

    painter.drawText(
        QPointF(
            vehicleScreen.x() + 20,
            vehicleScreen.y() - 30),

        QString("eY=%1 m  steer=%2 deg")
            .arg(trackingLateralError_, 0, 'f', 2)
            .arg(steeringDegree, 0, 'f', 1));

    painter.restore();
}

void View2D::showReplayFrame(double x, double y, double yaw)
{
    vehicleX_ = x;
    vehicleY_ = y;
    vehicleYaw_ = yaw;

    update();
}