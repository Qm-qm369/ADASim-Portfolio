#include "HeadlessRunner.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QSaveFile>
#include <QSettings>

#include "communication/SerialPort.h"
#include "communication/SerialProtocol.h"
#include "backend/DataLoader.h"
#include "backend/DataManager.h"
#include "communication/Socket.h"
#include "system/LinuxLogger.h"
#include "communication/PlannerLink.h"

static QString aebExpectationToString(AebExpectation expectation);

// 启动一个无界面模式的 ADASim，把配置、数据时钟、数据管理、仿真核心、TCP 网络这些后台模块创建起来并连接好。
HeadlessRunner::HeadlessRunner(
    const QString &configPath,
    const QString &dataPath,
    const QString &scenarioPath,
    QObject *parent)
    : QObject(parent),
      configPath_(configPath),
      dataPath_(dataPath),
      scenarioPath_(scenarioPath)
{
}

HeadlessRunner::~HeadlessRunner()
{
    shutdown();

    LinuxLogger::info(
        "ADASim headless mode stopped");

    LinuxLogger::shutdown();
}

bool HeadlessRunner::initialize()
{
    if (initialized_)
    {
        return true;
    }

    LinuxLogger::init();

    LinuxLogger::info(
        "ADASim headless mode initializing");

    // 1. 加载配置
    if (!loadConfig())
    {
        return false;
    }

    // 2. 创建数据时钟
    dataLoader_ =
        new DataLoader(
            dataPath_,
            this);

    // 3. 创建数据管理模块
    dataManager_ =
        new DataManager(this);

    // 4. 创建仿真核心
    simulationEngine_ =
        new SimulationEngine(this);

    // 使用 INI 参数配置 Engine
    simulationEngine_->configure(
        appConfig_.targetSpeed,
        appConfig_.planningDistance,
        appConfig_.lookAheadDistance,
        appConfig_.headingGain,
        appConfig_.lateralGain,
        appConfig_.controllerMode ==
            "PurePursuit");

    // 5. 创建 TCP Server
    socketServer_ =
        new SocketServer(this);

    plannerLink_ = new PlannerLink(this);

    // 6. 建立连接
    setupConnections();

    if (!loadScenario())
    {
        LinuxLogger::warning(
            "No valid scenario loaded");

        return false;
    }

    if (!setupNetwork())
    {
        return false;
    }

    if (!simulationEngine_->startRecording("record/simulation.csv"))
    {
        qCritical() << "[INIT] Cannot start recording";
        return false;
    }

    LinuxLogger::info(
        "ADASim headless initialized");

    if (!setupSerial())
    {
        return false;
    }

    initialized_ = true;

    return true;
}

bool HeadlessRunner::loadConfig()
{
    QString errorMessage;

    AppConfig loadedConfig;

    bool success =
        ConfigManager::load(
            configPath_,
            loadedConfig,
            errorMessage);

    if (success)
    {
        appConfig_ =
            loadedConfig;

        LinuxLogger::info(
            QString(
                "Config loaded: %1")
                .arg(configPath_));

        return true;
    }
    else
    {
        LinuxLogger::warning(
            errorMessage);

        qCritical().noquote() << "[CONFIG]" << errorMessage;
        return false;
    }
}

void HeadlessRunner::applyScenarioTestConfig()
{
    if (scenarioPath_.isEmpty())
    {
        testAebExpectation_ = AebExpectation::Any;
        maxTestFrames_ = 200;
        return;
    }

    testAebExpectation_ = scenario_.test.aebExpectation;
    maxTestFrames_ = scenario_.test.maxFrames;
}

bool HeadlessRunner::loadScenario()
{
    // 没有指定scenario
    // 保持V2.4行为
    if (scenarioPath_.isEmpty())
    {
        qInfo() << "[SCENARIO]" << "No scenario specified";
        applyScenarioTestConfig();
        return true;
    }

    QString errorMessage;

    bool success =
        ScenarioLoader::load(
            scenarioPath_,
            scenario_,
            errorMessage);

    if (!success)
    {
        LinuxLogger::error(
            errorMessage);

        qCritical().noquote()
            << "[SCENARIO]"
            << errorMessage;

        return false;
    }

    qInfo().noquote()
        << QString(
               "[SCENARIO] Loaded: %1 "
               "obstacles=%2")
               .arg(
                   scenario_.name)
               .arg(
                   scenario_.obstacles.size());

    for (const QPointF &point :
         scenario_.obstacles)
    {
        qInfo()
            << "[SCENARIO]"
            << "Obstacle:"
            << point.x()
            << point.y();

        dataManager_->onUserObstacleAdded(
            point.x(),
            point.y());
    }

    scenarioLoaded_ = true;

    applyScenarioTestConfig();

    qInfo() << "[TEST] max_frames:" << maxTestFrames_
            << "aeb_expectation:"
            << (testAebExpectation_ == AebExpectation::Required
                    ? "Required"
                : testAebExpectation_ == AebExpectation::Forbidden
                    ? "Forbidden"
                    : "Any");

    return true;
}

void HeadlessRunner::setupConnections()
{
    // ==========================================
    // DataLoader -> SimulationEngine
    // ==========================================

    connect(
        dataLoader_,
        &DataLoader::simulationTick,
        simulationEngine_,
        &SimulationEngine::onSimulationTick);

    // ==========================================
    // SimulationEngine -> DataManager
    // ==========================================

    connect(
        simulationEngine_,
        &SimulationEngine::simulationFrameReady,
        dataManager_,
        &DataManager::onSimulationFrame);

    // ==========================================
    // Engine状态 -> Headless终端
    // ==========================================

    connect(
        simulationEngine_,
        &SimulationEngine::frameUpdated,
        this,
        &HeadlessRunner::onSimulationFrameUpdated);

    // ==========================================
    // DataManager -> Engine
    // ==========================================

    connect(
        dataManager_,
        &DataManager::frontObstacleDistanceUpdated,
        simulationEngine_,
        &SimulationEngine::onFrontObstacleDistanceUpdated);

    // ==========================================
    // DataLoader 状态
    // ==========================================

    connect(
        dataLoader_,
        &DataLoader::statusUpdate,
        this,
        &HeadlessRunner::onStatusUpdate);

    connect(
        socketServer_,
        &SocketServer::networkError,
        this,
        [](const QString &message)
        {
            LinuxLogger::error(
                message);

            qCritical().noquote()
                << "[NETWORK]"
                << message;
        });

    connect(
        socketServer_,
        &SocketServer::clientConnected,
        this,
        []()
        {
            LinuxLogger::info(
                "Python Planner connected");

            qInfo()
                << "[NETWORK]"
                << "Python Planner connected";
        });

    connect(
        socketServer_,
        &SocketServer::clientDisconnected,
        this,
        []()
        {
            LinuxLogger::warning(
                "Python Planner disconnected");

            qWarning()
                << "[NETWORK]"
                << "Python Planner disconnected";
        });

    connect(simulationEngine_, &SimulationEngine::plannerRunChanged,
            plannerLink_, &PlannerLink::setRun);
    connect(socketServer_, &SocketServer::clientConnected,
            plannerLink_, &PlannerLink::onConnected);
    connect(socketServer_, &SocketServer::clientDisconnected,
            plannerLink_, &PlannerLink::onDisconnected);
    connect(dataManager_, &DataManager::plannerDataReady,
            plannerLink_, &PlannerLink::onPerception);
    connect(plannerLink_, &PlannerLink::sendRequested,
            socketServer_, &SocketServer::sendToClient);
    connect(socketServer_, &SocketServer::dataReceived,
            plannerLink_, &PlannerLink::onReply);
    connect(plannerLink_, &PlannerLink::controlReady,
            simulationEngine_, &SimulationEngine::onLateralControlReceived);
    connect(plannerLink_, &PlannerLink::protocolError,
            this, [](const QString &message)
            {
                LinuxLogger::warning(message);
                qWarning().noquote() << "[PLANNER]" << message; });
}

bool HeadlessRunner::setupNetwork()
{
    if (appConfig_.plannerPort < 1 || appConfig_.plannerPort > 65535)
    {
        qCritical() << "[NETWORK] Invalid port:" << appConfig_.plannerPort;
        return false;
    }

    bool success =
        socketServer_->startTcpServer(
            static_cast<quint16>(
                appConfig_.plannerPort));

    if (success)
    {
        LinuxLogger::info(
            QString(
                "Planner TCP Server listening on %1")
                .arg(
                    appConfig_.plannerPort));

        qInfo()
            << "[NETWORK]"
            << "Planner TCP Server:"
            << appConfig_.plannerPort;
    }
    else
    {
        LinuxLogger::error(
            "Planner TCP Server start failed");

        qCritical()
            << "[NETWORK]"
            << "TCP Server start failed";
    }

    return success;
}

void HeadlessRunner::start()
{
    if (started_)
    {
        return;
    }

    if (!initialized_)
    {
        qCritical()
            << "[ADASim]"
            << "Runner not initialized";

        return;
    }

    started_ = true;

    LinuxLogger::info(
        "ADASim headless simulation started");

    qInfo()
        << "[ADASim]"
        << "Headless simulation started";

    // 先初始化 Engine
    simulationEngine_->start();

    // 再启动100ms Tick
    dataLoader_->start();
}

void HeadlessRunner::onSimulationFrameUpdated(
    const SimulationFrame &frame,
    int currentIndex,
    bool targetSettled)
{
    latestSpeedMps_ = frame.speedKmH / 3.6;
    latestAeb_ = frame.emergencyBrake;
    haveSerialState_ = true;

    Q_UNUSED(currentIndex);

    ++frameCounter_;

    if (testMode_ &&
        frameCounter_ >= maxTestFrames_ &&
        !testFinished_)
    {
        testFinished_ = true;

        dataLoader_->pause();

        QTimer::singleShot(0, this, [this]()
                           { finishTest(); });
    }

    // DataLoader是10Hz
    // 每10帧打印一次，大约每秒一次
    if (frameCounter_ % 10 != 0)
    {
        return;
    }

    QString controllerName =
        frame.controllerMode == 0
            ? "DualError"
            : "PurePursuit";

    qInfo().noquote()
        << QString(
               "[SIM] x=%1 m  y=%2 m  "
               "speed=%3 km/h  "
               "controller=%4  "
               "TTC=%5 s  "
               "AEB=%6")
               .arg(
                   frame.vehicle.x,
                   0,
                   'f',
                   2)
               .arg(
                   frame.vehicle.y,
                   0,
                   'f',
                   2)
               .arg(
                   frame.speedKmH,
                   0,
                   'f',
                   1)
               .arg(
                   controllerName)
               .arg(
                   frame.ttc,
                   0,
                   'f',
                   2)
               .arg(
                   frame.emergencyBrake
                       ? "ON"
                       : "OFF");

    if (targetSettled)
    {
        qInfo()
            << "[SIM]"
            << "Lateral target settled";
    }
}

void HeadlessRunner::onStatusUpdate(
    const QString &status)
{
    qInfo().noquote()
        << "[STATUS]"
        << status;
}
void HeadlessRunner::onTerminationRequested(
    int signalNumber)
{
    LinuxLogger::warning(
        QString(
            "Termination signal received: %1")
            .arg(signalNumber));

    qInfo()
        << "[SYSTEM]"
        << "Termination signal:"
        << signalNumber;

    shutdown();

    if (testMode_)
    {
        qCritical() << "[TEST] Aborted before completion";
        QCoreApplication::exit(2);
    }
    else
    {
        QCoreApplication::quit();
    }
}

void HeadlessRunner::shutdown()
{
    if (shutdownStarted_)
        return;

    shutdownStarted_ = true;

    if (serialPort_)
    {
        serialPort_->closeDevice();
    }

    if (dataLoader_)
        dataLoader_->stop();

    if (simulationEngine_)
        simulationEngine_->stop();

    if (socketServer_)
        socketServer_->stop();
}

void HeadlessRunner::setTestMode(
    bool enable)
{
    testMode_ = enable;
}

bool HeadlessRunner::saveTestReport(
    const QString &scenarioName,
    const ScenarioTestConfig &testConfig,
    const TestResult &result)
{
    if (!QDir().mkpath("test_result"))
    {
        qCritical()
            << "[TEST] Cannot create test_result directory";

        return false;
    }

    QSaveFile file(
        "test_result/report.txt");

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text))
    {
        qCritical()
            << "[TEST] Cannot open report:"
            << file.errorString();

        return false;
    }

    QTextStream out(&file);

    out << "ADASim Test Report\n\n"
        << "Scenario:"
        << scenarioName
        << "\n"
        << "Expected AEB:"
        << aebExpectationToString(
               testConfig.aebExpectation)
        << "\n"
        << "Max Frames:"
        << testConfig.maxFrames
        << "\n"
        << "Actual Frames:"
        << result.frameCount
        << "\n"
        << "PASS:"
        << result.passed
        << "\n"
        << "AEB:"
        << result.aebTriggered
        << "\n"
        << "Collision:"
        << result.collision
        << "\n"
        << "Min TTC:"
        << result.minTtc
        << "\n";

    out.flush();

    if (out.status() != QTextStream::Ok)
    {
        qCritical()
            << "[TEST] Cannot write report:"
            << file.errorString();

        file.cancelWriting();
        return false;
    }

    if (!file.commit())
    {
        qCritical()
            << "[TEST] Cannot commit report:"
            << file.errorString();

        return false;
    }

    return true;
}

void HeadlessRunner::finishTest()
{
    if (shutdownStarted_)
        return;

    const TestResult result =
        simulationEngine_->testResult(testAebExpectation_);

    ScenarioTestConfig testConfig;
    testConfig.maxFrames = maxTestFrames_;
    testConfig.aebExpectation = testAebExpectation_;

    const QString scenarioName =
        scenarioPath_.isEmpty() ? QStringLiteral("default")
                                : scenario_.name;

    const bool reportSaved =
        saveTestReport(scenarioName, testConfig, result);

    shutdown();

    if (!reportSaved)
    {
        qCritical() << "[TEST] ERROR: report was not saved";
        QCoreApplication::exit(2);
    }
    else
    {
        qInfo() << "[TEST]" << (result.passed ? "PASS" : "FAIL");
        QCoreApplication::exit(result.passed ? 0 : 1);
    }
}

static QString aebExpectationToString(
    AebExpectation expectation)
{
    switch (expectation)
    {
    case AebExpectation::Any:
        return "Any";

    case AebExpectation::Required:
        return "Required";

    case AebExpectation::Forbidden:
        return "Forbidden";
    }

    return "Unknown";
}

// 读取串口配置，创建串口对象，连接收到数据后的处理函数，最后打开串口。
bool HeadlessRunner::setupSerial()
{
    QSettings settings(configPath_, QSettings::IniFormat);

    const bool enabled =
        settings.value("serial/enabled", false).toBool();

    if (!enabled)
    {
        return true;
    }

    const QString device =
        settings.value("serial/device").toString().trimmed();

    if (device.isEmpty())
    {
        qCritical() << "[SERIAL] Device path is empty";
        return false;
    }

    serialTargetSpeedMps_ = appConfig_.targetSpeed;
    serialPort_ = new SerialPort(this);

    connect(serialPort_, &SerialPort::lineReceived,
            this, &HeadlessRunner::handleSerialLine);

    connect(serialPort_, &SerialPort::lineRejected,
            this, [this](const QString &reason)
            { serialPort_->sendLine(
                  QByteArray("ERR ") + reason.toUtf8()); });

    connect(serialPort_, &SerialPort::portError,
            this, [](const QString &message)
            { qWarning().noquote()
                  << "[SERIAL] Closed:" << message; });

    QString error;

    if (!serialPort_->openDevice(device, error))
    {
        qCritical().noquote()
            << "[SERIAL] Cannot open" << device << error;
        return false;
    }

    qInfo().noquote()
        << "[SERIAL] Opened:" << device;

    return true;
}

// 收到一条完整串口命令后，判断要做什么，执行操作，再给对端回复。
void HeadlessRunner::handleSerialLine(
    const QByteArray &line)
{
    if (shutdownStarted_)
    {
        return;
    }

    const SerialCommand command =
        parseSerialCommand(line.toStdString());

    switch (command.type)
    {
    case CommandType::Ping:
        serialPort_->sendLine("PONG");
        break;

    case CommandType::SetSpeed:
        serialTargetSpeedMps_ = command.speedMps;

        simulationEngine_->setTargetVehicleSpeed(
            command.speedMps);

        serialPort_->sendLine(
            QByteArray("OK SET_SPEED ") +
            QByteArray::number(command.speedMps, 'f', 3));

        qInfo() << "[SERIAL] Target speed:"
                << command.speedMps << "m/s";
        break;

    case CommandType::GetState:
        if (!haveSerialState_)
        {
            serialPort_->sendLine("ERR state-not-ready");
            break;
        }

        serialPort_->sendLine(
            QByteArray("STATE target_speed_mps=") +
            QByteArray::number(
                serialTargetSpeedMps_, 'f', 3) +
            " speed_mps=" +
            QByteArray::number(latestSpeedMps_, 'f', 3) +
            " aeb=" +
            (latestAeb_ ? "1" : "0"));
        break;

    case CommandType::Invalid:
        serialPort_->sendLine(
            QByteArray("ERR ") +
            QByteArray::fromStdString(command.error));
        break;
    }
}