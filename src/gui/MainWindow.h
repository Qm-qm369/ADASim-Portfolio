#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QLabel>
#include <QSlider>
#include <QVector>
#include <QPointF>
#include <cmath>
#include <QComboBox>
#include <QDoubleSpinBox>

#include "View2D.h"
#include "backend/DataLoader.h"
#include "backend/DataManager.h"
#include "SensorView.h"
#include "communication/Socket.h"
#include "ControlMonitor.h"
#include "system/LinuxLogger.h"
#include "config/ConfigManager.h"
#include "backend/SimulationEngine.h"

class QLayout;
class View2D;
class DataLoader;
class QThread;
class QCloseEvent;
class DataManager;
class SensorView;
class SocketServer;
class PlannerLink;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &configPath,
                        const QString &dataPath,
                        QWidget *parent = nullptr);
    ~MainWindow();

signals:

    void startEngineRequested();

    void stopEngineRequested();

    void replayFrameRequested(
        int index);

    void controllerGainsChanged(
        double headingGain,
        double lateralGain);

    void pauseEngineRequested();

private:
    void setupUI();

    /*
     ┌─────────────────────────────┐
    │      menuBar() 菜单栏        │ ← 最顶部，文件、编辑
    ├─────────────────────────────┤
    │ addToolBar() 工具栏区域      │ ← 可以顶部/左边/右边
    ├─────────────────────────────┤
    │                             │
    │     centralWidget()         │ ← 【中心部件】你的绘图画布放这里！最重要
    │      (你的绘图widget)        │
    │                             │
    ├─────────────────────────────┤
    │     statusBar() 状态栏       │ ← 死死固定在窗口最底部
    └─────────────────────────────┘

    */

    void setupToolBar();   // 创建顶部工具栏
    void setupStatusBar(); // 创建底部状态栏
    void setupConnections();

    // 创建顶部信息卡片
    void createInfoCard(QLayout *layout,
                        const QString &title,
                        const QString &value,
                        const QString &objectName);

    // 启动后台线程
    void startBackend();
    // 安全停止后台线程
    void stopBackend();

    // V1.9
    void loadConfig();
    void applyConfig();
    void saveConfig();

    void shutdownApplication();

protected:
    // 窗口关闭时安全结束后台线程
    void closeEvent(QCloseEvent *event) override;

    // V0.10
    void setupNetwork();

private slots:

    void onStartSimulation();
    void onPauseSimulation();
    void onStopSimulation();
    void onStatusUpdate(const QString &status);

    void onReplayFrameSelected(int index);

    void onSimulationFrameUpdated(
        const SimulationFrame &frame,
        int currentIndex,
        bool targetSettled);

    void onReplayFrameReady(
        int index,
        int maxIndex,
        const SimulationFrame &frame);

public slots:
    void onTerminationRequested(int signalNumber);

private:
    // 行车记录仪时间轴
    QSlider *timeSlider_ = nullptr;
    // 显示 当前帧 / 最大帧
    QLabel *timeLabel_ = nullptr;

    // 顶部数据显示控件
    QLabel *speedValue_ = nullptr;
    QLabel *distanceValue_ = nullptr;
    QLabel *fpsValue_ = nullptr;
    QLabel *algoValue_ = nullptr;

    // 后台数据线程
    QThread *backendThread_ = nullptr;
    // 数据生成器
    DataLoader *dataLoader_ = nullptr;

    DataManager *dataManager_ = nullptr;

    SensorView *sensorView_ = nullptr;

    ControlMonitor *controlMonitor_ = nullptr;

    SimulationEngine *simulationEngine_ = nullptr;

    View2D *view2D_ = nullptr;

    QString configPath_;
    QString dataPath_;

    // V0.10 TCP服务器
    SocketServer *socketServer_ = nullptr;

    QComboBox *controllerCombo_ = nullptr; // 控制器 QComboBox 下拉选择框

    QDoubleSpinBox *speedSpin_ = nullptr;     // 车辆速度 QDoubleSpinBox 浮点数输入框
    QDoubleSpinBox *lookAheadSpin_ = nullptr; // 前视距离

    QDoubleSpinBox *headingGainSpin_ = nullptr; // 航向误差权重
    QDoubleSpinBox *lateralGainSpin_ = nullptr; // 横向误差权重

    // V1.9运行配置
    AppConfig appConfig_;

    // Planner监听端口
    int plannerPort_ = 8080;

    bool shutdownStarted_ = false;

    PlannerLink *plannerLink_ = nullptr;
};

#endif
