#ifndef HEADLESSRUNNER_H
#define HEADLESSRUNNER_H

#include <QObject>
#include <QString>
#include <QByteArray>

#include "config/ConfigManager.h"
#include "scenario/ScenarioLoader.h"
#include "backend/SimulationEngine.h"

// 前向声明。只需要知道有这么一个类 暂时不需要类里面长什么样
class DataLoader;
class DataManager;
class SocketServer;
class PlannerLink;
class SerialPort;

class HeadlessRunner : public QObject
{
    Q_OBJECT

public:
    explicit HeadlessRunner(
        const QString &configPath,
        const QString &dataPath,
        const QString &scenarioPath,
        QObject *parent = nullptr);

    ~HeadlessRunner();

    // V2.4 新增：
    // 启动 Headless 仿真
    void start();

    bool initialize();

    void setTestMode(
        bool enable);

    void finishTest();

public slots:

    // LinuxSignalHandler 发来 SIGINT / SIGTERM 后执行
    void onTerminationRequested(
        int signalNumber);

private slots:

    // Engine 每计算完一帧后，用于终端显示
    void onSimulationFrameUpdated(
        const SimulationFrame &frame,
        int currentIndex,
        bool targetSettled);

    // DataLoader 状态输出
    void onStatusUpdate(
        const QString &status);

private:
    bool loadConfig();
    bool loadScenario();

    void setupConnections();

    bool setupNetwork();

    void shutdown();

    bool saveTestReport(
        const QString &scenarioName,
        const ScenarioTestConfig &testConfig,
        const TestResult &result);

    void applyScenarioTestConfig();

    bool setupSerial();
    void handleSerialLine(const QByteArray &line);

private:
    QString configPath_;
    QString dataPath_;
    QString scenarioPath_;

    AppConfig appConfig_;
    Scenario scenario_;

    DataLoader *dataLoader_ = nullptr;

    DataManager *dataManager_ = nullptr;

    SimulationEngine *simulationEngine_ = nullptr;

    SocketServer *socketServer_ = nullptr;

    bool initialized_ = false;
    bool started_ = false;

    bool shutdownStarted_ = false;

    bool scenarioLoaded_ = false;

    // 控制终端打印频率
    int frameCounter_ = 0;

    bool testMode_ = false;
    bool testFinished_ = false;

    AebExpectation testAebExpectation_ =
        AebExpectation::Any;

    int maxTestFrames_ = 200;

    PlannerLink *plannerLink_ = nullptr;

    SerialPort *serialPort_ = nullptr;

    // 用户请求的目标速度，单位 m/s。
    double serialTargetSpeedMps_ = 0.0;

    // 最新仿真帧中的实际状态。
    double latestSpeedMps_ = 0.0;
    bool latestAeb_ = false;
    bool haveSerialState_ = false;
};

#endif