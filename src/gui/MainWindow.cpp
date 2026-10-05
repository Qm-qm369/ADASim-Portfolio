#include "MainWindow.h"
#include "communication/PlannerLink.h"

#include <QThread>
#include <QDebug>
#include <QCloseEvent>
#include <QMetaObject>
#include <QToolBar>
#include <QStatusBar>
#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>

MainWindow::MainWindow(const QString &configPath,
                       const QString &dataPath,
                       QWidget *parent)
    : QMainWindow(parent), configPath_(configPath), dataPath_(dataPath)
{
    LinuxLogger::init();

    LinuxLogger::info("ADASim application started");

    setWindowTitle(
        QString("ADASim - 自动驾驶算法仿真平台 v%1")
            .arg(QCoreApplication::applicationVersion()));
    resize(1600, 900);

    setStyleSheet("QMainWindow { background-color: #050811; }");

    // V1.9
    loadConfig();
    applyConfig();

    // V2.3：创建独立仿真核心
    simulationEngine_ =
        new SimulationEngine(this);

    simulationEngine_->configure(
        appConfig_.targetSpeed,
        appConfig_.planningDistance,
        appConfig_.lookAheadDistance,
        appConfig_.headingGain,
        appConfig_.lateralGain,
        appConfig_.controllerMode ==
            "PurePursuit");

    setupUI();
    setupToolBar();
    setupStatusBar();

    // 创建并启动后台线程
    startBackend();
    // 建立数据连接
    setupConnections();

    // V0.10网络与规划
    setupNetwork();
}

MainWindow::~MainWindow()
{
    shutdownApplication();

    LinuxLogger::info("ADASim application stopped");

    LinuxLogger::shutdown();
}

void MainWindow::loadConfig()
{
    QString errorMessage;

    AppConfig loadedConfig;

    bool success = ConfigManager::load(configPath_, loadedConfig, errorMessage);

    if (success)
    {
        appConfig_ = loadedConfig;

        LinuxLogger::info(QString("Config loaded: %1").arg(configPath_));
    }
    else
    {
        LinuxLogger::warning(errorMessage);
    }
}

void MainWindow::applyConfig()
{
    plannerPort_ =
        appConfig_.plannerPort;
}

void MainWindow::startBackend()
{
    qDebug() << "MainWindow thread ="
             << QThread::currentThread();
    // 创建后台线程
    backendThread_ = new QThread(this);

    /*
     * 注意：
     * DataLoader 这里不能传 this 作为 parent。
     * Qt 要保证：父对象和子对象必须属于同一个线程。
     * 因为有 parent 的 QObject
     * 不能被 moveToThread() 移到其他线程。
     */
    dataLoader_ = new DataLoader(dataPath_);
    // 将 DataLoader 的线程归属移动到后台线程
    dataLoader_->moveToThread(backendThread_);

    dataManager_ = new DataManager();
    dataManager_->moveToThread(backendThread_);

    // 后台线程结束时，自动删除 DataLoader
    connect(backendThread_, &QThread::finished,
            dataLoader_, &QObject::deleteLater);

    connect(backendThread_, &QThread::finished,
            dataManager_, &QObject::deleteLater);

    // 启动后台线程事件循环
    backendThread_->start();
}

void MainWindow::stopBackend()
{
    if (backendThread_ &&
        backendThread_->isRunning())
    {
        /*
         * 先让 DataLoader 在自己的线程中停掉 QTimer。
         *
         * BlockingQueuedConnection：
         * 主线程会等待 stop() 真正执行完再继续。
         */
        QMetaObject::invokeMethod(
            dataLoader_,
            "stop",
            Qt::BlockingQueuedConnection);

        // 通知后台线程退出事件循环
        backendThread_->quit();

        // 等待后台线程真正结束
        if (!backendThread_->wait(2000))
        {
            qWarning() << "[THREAD] Still stopping; waiting for cleanup";
            backendThread_->wait();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    shutdownApplication();

    // 接受这个事件，事件不再继续向上传递。
    event->accept();
}

// 建立真正的数据连接
void MainWindow::setupConnections()
{

    connect(
        dataLoader_,
        &DataLoader::simulationTick,
        simulationEngine_,
        &SimulationEngine::onSimulationTick);

    connect(
        simulationEngine_,
        &SimulationEngine::simulationFrameReady,
        dataManager_,
        &DataManager::onSimulationFrame);

    connect(
        simulationEngine_,
        &SimulationEngine::frameUpdated,
        this,
        &MainWindow::onSimulationFrameUpdated);

    connect(
        simulationEngine_,
        &SimulationEngine::liveVehicleStateReady,
        view2D_,
        &View2D::showReplayFrame);

    connect(
        simulationEngine_,
        &SimulationEngine::replayFrameReady,
        this,
        &MainWindow::onReplayFrameReady);

    connect(
        this,
        &MainWindow::startEngineRequested,
        simulationEngine_,
        &SimulationEngine::start);

    connect(
        this,
        &MainWindow::stopEngineRequested,
        simulationEngine_,
        &SimulationEngine::stop);

    connect(
        this,
        &MainWindow::replayFrameRequested,
        simulationEngine_,
        &SimulationEngine::requestReplayFrame);

    connect(
        dataManager_,
        &DataManager::frontObstacleDistanceUpdated,
        simulationEngine_,
        &SimulationEngine::onFrontObstacleDistanceUpdated);

    // DataLoader -> 状态栏
    connect(dataLoader_, &DataLoader::statusUpdate, this, &MainWindow::onStatusUpdate);

    connect(timeSlider_, &QSlider::valueChanged, this, &MainWindow::onReplayFrameSelected);

    // 最终点云给SensorView
    connect(
        dataManager_,
        &DataManager::mergedPointCloudReady,
        sensorView_,
        &SensorView::updatePointCloud);

    // 检测出的障碍物给View2D
    connect(
        dataManager_,
        &DataManager::obstaclesDetected,
        view2D_,
        &View2D::updateObstacles);

    // View2D右键添加障碍物
    connect(
        view2D_,
        &View2D::userObstacleAdded,
        dataManager_,
        &DataManager::onUserObstacleAdded);

    connect(dataManager_, &DataManager::pathPredicted,
            this, [this](const QVector<QPointF> &path)
            {
            if (algoValue_)
            {
                algoValue_->setText(QString("预测 %1 点").arg(path.size()));
            } });

    connect(dataManager_, &DataManager::pathPredicted,
            view2D_, &View2D::updatePredictedPath);

    // 当下拉框 controllerCombo_ 的当前选项发生变化时，把新的选项索引 int 传给 simulationEngine_ 的 setControllerModeIndex()。
    connect(
        controllerCombo_,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged),
        simulationEngine_,
        &SimulationEngine::setControllerModeIndex);

    connect(
        speedSpin_,
        QOverload<double>::of(
            &QDoubleSpinBox::valueChanged),
        simulationEngine_,
        &SimulationEngine::setTargetVehicleSpeed);

    connect(
        lookAheadSpin_,
        QOverload<double>::of(
            &QDoubleSpinBox::valueChanged),
        simulationEngine_,
        &SimulationEngine::setLookAheadDistance);

    connect(
        headingGainSpin_,
        QOverload<double>::of(
            &QDoubleSpinBox::valueChanged),
        this,
        [this](double)
        {
            emit controllerGainsChanged(
                headingGainSpin_->value(),
                lateralGainSpin_->value());
        });

    connect(
        lateralGainSpin_,
        QOverload<double>::of(
            &QDoubleSpinBox::valueChanged),
        this,
        [this](double)
        {
            emit controllerGainsChanged(
                headingGainSpin_->value(),
                lateralGainSpin_->value());
        });

    connect(
        this,
        &MainWindow::controllerGainsChanged,
        simulationEngine_,
        &SimulationEngine::setControllerGains);

    connect(
        dataManager_,
        &DataManager::frontObstacleDistanceUpdated,
        simulationEngine_,
        &SimulationEngine::onFrontObstacleDistanceUpdated);

    connect(this, &MainWindow::pauseEngineRequested,
            simulationEngine_, &SimulationEngine::pause);
}

void MainWindow::onStartSimulation()
{
    emit startEngineRequested();

    QMetaObject::invokeMethod(
        dataLoader_,
        "start",
        Qt::QueuedConnection);
}

void MainWindow::onPauseSimulation()
{
    emit pauseEngineRequested();

    QMetaObject::invokeMethod(
        dataLoader_, "pause", Qt::QueuedConnection);
}

void MainWindow::onStopSimulation()
{
    QMetaObject::invokeMethod(dataLoader_, "stop", Qt::QueuedConnection);

    if (speedValue_)
    {
        speedValue_->setText("0.0 km/h");
    }

    if (distanceValue_)
    {
        distanceValue_->setText("0.0 m");
    }

    if (view2D_)
    {
        view2D_->updateVehiclePosition(0.0, 0.0, 0.0);
        view2D_->updatePlannedTrajectory(QVector<QPointF>());
    }

    timeSlider_->blockSignals(true);
    timeSlider_->setMinimum(0);
    timeSlider_->setMaximum(0);
    timeSlider_->setValue(0);
    timeSlider_->blockSignals(false);

    timeLabel_->setText("0 / 0");

    if (view2D_)
    {
        view2D_->updateVehiclePosition(0.0, 0.0, 0.0);
        view2D_->updatePlannedTrajectory(QVector<QPointF>());
        view2D_->clearTrackingDebug();
    }

    if (controlMonitor_)
    {
        controlMonitor_->clear();
    }

    emit stopEngineRequested();
}

void MainWindow::onStatusUpdate(const QString &status)
{
    statusBar()->showMessage(status);
}

/**
 * 创建 V0.2 的静态主界面
 */
void MainWindow::setupUI()
{
    // QMainWindow 的中央区域
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 整个页面采用上下排列
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // =========================
    // 1. 顶部信息区域
    // =========================

    QFrame *infoPanel = new QFrame(this); // QFrame非常适合当卡片 面板 边框区域 分组容器
    infoPanel->setFixedHeight(100);

    infoPanel->setStyleSheet(
        "QFrame {"
        "background-color: #111827;"
        "border: 1px solid #2d3748;"
        "border-radius: 8px;"
        "}");

    // 四张卡片左右排列
    QHBoxLayout *infoLayout = new QHBoxLayout(infoPanel);
    infoLayout->setSpacing(15);

    createInfoCard(infoLayout,
                   "当前速度",
                   "0 km/h",
                   "speedValue");

    createInfoCard(infoLayout,
                   "行驶里程",
                   "0.0 m",
                   "distanceValue");

    createInfoCard(infoLayout,
                   "渲染帧率",
                   "60 FPS",
                   "fpsValue");

    createInfoCard(infoLayout,
                   "算法状态",
                   "就绪",
                   "algoValue");

    mainLayout->addWidget(infoPanel);

    // =========================
    // 1.5. 控制面板
    // =========================

    QFrame *controlPanel = new QFrame(this);
    controlPanel->setObjectName("controlPanel");
    controlPanel->setFixedHeight(110);

    controlPanel->setStyleSheet(
        "QFrame#controlPanel {"
        "    background-color: #0f172a;"
        "    border: 1px solid #243041;"
        "    border-radius: 10px;"
        "}"
        "QWidget#paramCard {"
        "    background-color: #111827;"
        "    border: 1px solid #2d3748;"
        "    border-radius: 8px;"
        "}"
        "QLabel#paramTitle {"
        "    color: #94a3b8;"
        "    font-size: 13px;"
        "    font-weight: 600;"
        "    border: none;"
        "    background: transparent;"
        "}"
        "QComboBox, QDoubleSpinBox {"
        "    background-color: #0b1220;"
        "    color: #f8fafc;"
        "    border: 1px solid #3b475a;"
        "    border-radius: 6px;"
        "    padding: 6px 10px;"
        "    min-height: 32px;"
        "    font-size: 14px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 24px;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    width: 0px;"
        "    height: 0px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #0b1220;"
        "    color: #f8fafc;"
        "    border: 1px solid #3b475a;"
        "    selection-background-color: #1d4ed8;"
        "    selection-color: #ffffff;"
        "    outline: 0;"
        "    padding: 4px;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "    min-height: 28px;"
        "    padding: 6px 10px;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "    width: 18px;"
        "}");

    QHBoxLayout *controlLayout = new QHBoxLayout(controlPanel);
    controlLayout->setContentsMargins(12, 10, 12, 10);
    controlLayout->setSpacing(12);

    // 创建单个参数卡片的小工具函数
    auto createParamCard = [&](const QString &title, QWidget *editor) -> QWidget *
    {
        QFrame *card = new QFrame(controlPanel);
        card->setObjectName("paramCard");
        card->setMinimumWidth(180);

        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(6);

        QLabel *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("paramTitle");

        editor->setParent(card);

        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(editor);

        return card;
    };

    // ===== 创建控件 =====
    controllerCombo_ = new QComboBox();
    controllerCombo_->addItem("双误差控制");
    controllerCombo_->addItem("Pure Pursuit");

    if (appConfig_.controllerMode == "PurePursuit")
    {
        controllerCombo_->setCurrentIndex(1);
    }
    else
    {
        controllerCombo_->setCurrentIndex(0);
    }

    speedSpin_ = new QDoubleSpinBox();
    speedSpin_->setRange(1.0, 15.0);
    speedSpin_->setSingleStep(0.5);
    speedSpin_->setValue(appConfig_.targetSpeed);
    speedSpin_->setSuffix(" m/s");

    lookAheadSpin_ = new QDoubleSpinBox();
    lookAheadSpin_->setRange(1.0, 10.0);
    lookAheadSpin_->setSingleStep(0.5);
    lookAheadSpin_->setValue(appConfig_.lookAheadDistance);
    lookAheadSpin_->setSuffix(" m");

    headingGainSpin_ = new QDoubleSpinBox();
    headingGainSpin_->setRange(0.0, 5.0);
    headingGainSpin_->setSingleStep(0.1);
    headingGainSpin_->setValue(appConfig_.headingGain);

    lateralGainSpin_ = new QDoubleSpinBox();
    lateralGainSpin_->setRange(0.0, 5.0);
    lateralGainSpin_->setSingleStep(0.1);
    lateralGainSpin_->setValue(appConfig_.lateralGain);

    // ===== 放入面板 =====
    controlLayout->addWidget(createParamCard("控制器", controllerCombo_), 1);
    controlLayout->addWidget(createParamCard("速度", speedSpin_), 1);
    controlLayout->addWidget(createParamCard("前视距离", lookAheadSpin_), 1);
    controlLayout->addWidget(createParamCard("航向增益", headingGainSpin_), 1);
    controlLayout->addWidget(createParamCard("横向增益", lateralGainSpin_), 1);

    mainLayout->addWidget(controlPanel);
    // =========================
    // 2. 中间 2D 仿真区域
    // =========================

    QHBoxLayout *viewsLayout =
        new QHBoxLayout();

    view2D_ = new View2D(this);

    sensorView_ = new SensorView(this);

    controlMonitor_ = new ControlMonitor(this);

    QVBoxLayout *rightLayout = new QVBoxLayout();

    rightLayout->addWidget(sensorView_, 2);

    rightLayout->addWidget(controlMonitor_, 1);

    viewsLayout->addWidget(view2D_, 2);

    viewsLayout->addLayout(rightLayout, 1);

    mainLayout->addLayout(viewsLayout, 1);

    // =========================
    // 时间轴回放区域
    // =========================

    QFrame *playbackPanel = new QFrame(this);

    playbackPanel->setFixedHeight(55);

    playbackPanel->setStyleSheet(
        "QFrame {"
        "background-color: #111827;"
        "border: 1px solid #2d3748;"
        "border-radius: 6px;"
        "}");

    QHBoxLayout *playbackLayout =
        new QHBoxLayout(playbackPanel);

    QLabel *playbackTitle =
        new QLabel("时光回放：", playbackPanel);

    playbackTitle->setStyleSheet(
        "color: #8b949e;"
        "font-weight: bold;");

    // 横向滑动条
    timeSlider_ = new QSlider(Qt::Horizontal, playbackPanel);

    timeSlider_->setMinimum(0);
    timeSlider_->setMaximum(0);

    // 当前帧 / 最大帧
    timeLabel_ =
        new QLabel("0 / 0", playbackPanel);

    timeLabel_->setMinimumWidth(80);

    timeLabel_->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter);

    timeLabel_->setStyleSheet(
        "color: #8b949e;");

    playbackLayout->addWidget(playbackTitle);

    playbackLayout->addWidget(timeSlider_, 1);

    playbackLayout->addWidget(timeLabel_);

    mainLayout->addWidget(playbackPanel);
}

/**
 * 创建顶部信息卡片
 */
void MainWindow::createInfoCard(QLayout *layout,
                                const QString &title,
                                const QString &value,
                                const QString &objectName)
{
    QFrame *card = new QFrame(this);

    card->setStyleSheet(
        "QFrame {"
        "background-color: #161b22;"
        "border: 1px solid #30363d;"
        "border-radius: 6px;"
        "}");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "color: #8b949e;"
        "font-size: 13px;"
        "border: none;");

    QLabel *valueLabel = new QLabel(value, card);
    valueLabel->setObjectName(objectName);
    // objectName有两个用途
    // 1.这个 QLabel 设置 Qt 对象名。
    // 2.利用这个名字判断：这张卡片到底是速度、里程、FPS 还是算法状态。

    valueLabel->setStyleSheet(
        "color: #00ff88;"
        "font-size: 22px;"
        "font-weight: bold;"
        "border: none;");

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(valueLabel);

    layout->addWidget(card);

    // 保存数值 QLabel，后续版本需要动态修改
    if (objectName == "speedValue")
        speedValue_ = valueLabel;
    else if (objectName == "distanceValue")
        distanceValue_ = valueLabel;
    else if (objectName == "fpsValue")
        fpsValue_ = valueLabel;
    else if (objectName == "algoValue")
        algoValue_ = valueLabel;
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("主工具栏");

    toolBar->setMovable(false);

    /*
    这里 addAction() 帮我们直接把：
    点击工具栏按钮 然后调用槽函数 连接起来。
    */
    toolBar->addAction(
        "▶ 启动",
        this,
        &MainWindow::onStartSimulation);

    toolBar->addAction(
        "⏸ 暂停",
        this,
        &MainWindow::onPauseSimulation);

    toolBar->addAction(
        "⏹ 停止",
        this,
        &MainWindow::onStopSimulation);
}

void MainWindow::setupStatusBar()
{
    QStatusBar *bar = statusBar();

    // 底部状态栏高度
    bar->setMinimumHeight(32);

    // 单独设置状态栏样式，避免深色背景下文字看不清
    bar->setStyleSheet(
        "QStatusBar {"
        " background-color: #0b1220;"
        " color: #e2e8f0;"
        " border-top: 1px solid #1e293b;"
        " font-size: 13px;"
        " font-weight: 500;"
        " padding-left: 10px;"
        "}"
        "QStatusBar::item {"
        " border: none;"
        "}");

    bar->showMessage("ADASim 引擎就绪");
}

void MainWindow::setupNetwork()
{
    // =====================================
    // 1. 创建TCP服务器
    // =====================================

    socketServer_ = new SocketServer(this);
    plannerLink_ = new PlannerLink(this);

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

    connect(socketServer_, &SocketServer::networkError,
            this, [this](const QString &message)
            {
            statusBar()->showMessage( QString("网络异常：%1") .arg(message));

            LinuxLogger::error(message); });

    // =====================================
    // 6. 网络状态
    // =====================================

    connect(socketServer_, &SocketServer::clientConnected,
            this, [this]()
            {
            statusBar()->showMessage("● Python Planner 已连接");

            LinuxLogger::info("Python Planner connected"); });

    connect(socketServer_, &SocketServer::clientDisconnected,
            this, [this]()
            {
            statusBar()->showMessage( "● Python Planner 已断开，等待重新连接");

            LinuxLogger::warning("Python Planner disconnected"); });

    // =====================================
    // 7. 开始监听8080
    // =====================================

    bool success = socketServer_->startTcpServer(plannerPort_);

    if (success)
    {
        statusBar()->showMessage(QString("Planner TCP Server：%1").arg(plannerPort_));
    }
    else
    {
        statusBar()->showMessage("TCP Server启动失败");
    }
}

void MainWindow::saveConfig()
{
    appConfig_.targetSpeed =
        speedSpin_->value();

    appConfig_.lookAheadDistance =
        lookAheadSpin_->value();

    appConfig_.headingGain =
        headingGainSpin_->value();

    appConfig_.lateralGain =
        lateralGainSpin_->value();

    appConfig_.plannerPort =
        plannerPort_;

    if (controllerCombo_->currentIndex() == 1)
    {
        appConfig_.controllerMode =
            "PurePursuit";
    }
    else
    {
        appConfig_.controllerMode =
            "DualError";
    }

    QString errorMessage;

    bool success =
        ConfigManager::save(
            configPath_,
            appConfig_,
            errorMessage);

    if (success)
    {
        LinuxLogger::info(
            QString(
                "Config saved: %1")
                .arg(
                    configPath_));
    }
    else
    {
        LinuxLogger::warning(
            errorMessage);
    }
}

void MainWindow::onReplayFrameSelected(
    int index)
{
    onPauseSimulation();
    emit replayFrameRequested(index);
}

void MainWindow::shutdownApplication()
{
    // 防止重复执行退出流程
    if (shutdownStarted_)
    {
        return;
    }

    shutdownStarted_ = true;

    LinuxLogger::info("ADASim shutdown started");

    simulationEngine_->stop();

    // 1. 保存当前配置
    saveConfig();

    // 2. 停止后台仿真线程
    stopBackend();

    // 3. 停止TCP服务
    if (socketServer_)
    {
        socketServer_->stop();
    }

    LinuxLogger::info("ADASim shutdown completed");
}

void MainWindow::onTerminationRequested(int signalNumber)
{
    LinuxLogger::warning(QString("Termination signal received: %1").arg(signalNumber));

    // close()会正常触发closeEvent()
    close();
}

void MainWindow::onSimulationFrameUpdated(
    const SimulationFrame &frame,
    int currentIndex,
    bool targetSettled)
{
    // ==========================================
    // 车辆显示
    // ==========================================

    if (view2D_)
    {
        view2D_->updateVehiclePosition(
            frame.vehicle.x,
            frame.vehicle.y,
            frame.vehicle.yaw);

        view2D_->updateTrackingDebug(
            QPointF(
                frame.vehicle.x,
                frame.targetY),
            frame.lateralError,
            frame.steeringAngle);
    }

    // ==========================================
    // 控制曲线
    // ==========================================

    if (controlMonitor_)
    {
        controlMonitor_->appendSample(
            frame.lateralError,
            frame.steeringAngle);
    }

    // ==========================================
    // 时间轴
    // ==========================================

    timeSlider_->blockSignals(true);

    timeSlider_->setMaximum(
        currentIndex);

    timeSlider_->setValue(
        currentIndex);

    timeSlider_->blockSignals(false);

    timeLabel_->setText(
        QString("%1 / %2")
            .arg(currentIndex)
            .arg(currentIndex));

    // ==========================================
    // GUI 数据
    // ==========================================

    if (speedValue_)
    {
        speedValue_->setText(
            QString::number(
                frame.speedKmH,
                'f',
                1) +
            " km/h");
    }

    if (distanceValue_)
    {
        distanceValue_->setText(
            QString::number(
                frame.totalDistance,
                'f',
                1) +
            " m");
    }

    if (algoValue_)
    {
        double steeringDegree =
            frame.steeringAngle *
            180.0 /
            M_PI;

        if (targetSettled)
        {
            algoValue_->setText(
                QString(
                    "已稳定 %1 m")
                    .arg(
                        frame.vehicle.y,
                        0,
                        'f',
                        2));
        }
        else
        {
            QString controllerName =
                frame.controllerMode == 0
                    ? "双误差"
                    : "PurePursuit";

            algoValue_->setText(
                QString(
                    "%1  eY=%2m  steer=%3°")
                    .arg(
                        controllerName)
                    .arg(
                        frame.lateralError,
                        0,
                        'f',
                        2)
                    .arg(
                        steeringDegree,
                        0,
                        'f',
                        1));
        }
    }
}

void MainWindow::onReplayFrameReady(
    int index,
    int maxIndex,
    const SimulationFrame &frame)
{
    view2D_->showReplayFrame(
        frame.vehicle.x,
        frame.vehicle.y,
        frame.vehicle.yaw);

    view2D_->updateTrackingDebug(
        QPointF(
            frame.vehicle.x,
            frame.targetY),
        frame.lateralError,
        frame.steeringAngle);

    if (speedValue_)
    {
        speedValue_->setText(
            QString::number(
                frame.speedKmH,
                'f',
                1) +
            " km/h");
    }

    if (distanceValue_)
    {
        distanceValue_->setText(
            QString::number(
                frame.totalDistance,
                'f',
                1) +
            " m");
    }

    if (algoValue_)
    {
        double steeringDegree =
            frame.steeringAngle *
            180.0 /
            M_PI;

        double headingDegree =
            frame.headingError *
            180.0 /
            M_PI;

        algoValue_->setText(
            QString(
                "回放 eY=%1m eYaw=%2° steer=%3°")
                .arg(
                    frame.lateralError,
                    0,
                    'f',
                    2)
                .arg(
                    headingDegree,
                    0,
                    'f',
                    1)
                .arg(
                    steeringDegree,
                    0,
                    'f',
                    1));
    }

    timeLabel_->setText(
        QString("%1 / %2")
            .arg(index)
            .arg(maxIndex));
}