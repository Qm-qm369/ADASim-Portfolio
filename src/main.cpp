#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QString>
#include <QDebug>

#include <memory>

#include "gui/MainWindow.h"
#include "headless/HeadlessRunner.h"
#include "system/LinuxSignalHandler.h"
#include "Version.h"
#include "test/TestSuiteRunner.h"

// 在创建 QApplication / QCoreApplication 之前
// 先判断用户有没有传 --headless。
static bool hasHeadlessArgument(
    int argc,
    char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        // 把 C 风格字符串 char * 转成 Qt 的 QString。
        if (QString::fromLocal8Bit(
                argv[i]) ==
            "--headless")
        {
            return true;
        }
    }

    return false;
}

int main(
    int argc,
    char *argv[])
{
    // ==========================================
    // V2.4：决定使用GUI还是Headless应用
    // ==========================================

    bool useHeadlessApplication =
        hasHeadlessArgument(
            argc,
            argv);

    if (!useHeadlessApplication)
    {
        for (int i = 1; i < argc; ++i)
        {
            const QString arg = QString::fromLocal8Bit(argv[i]);
            if (arg == "--test")
            {
                qCritical() << "--test requires --headless";
                return 2;
            }
            if (arg == "--suite")
            {
                qCritical() << "--suite requires --headless";
                return 2;
            }
        }
    }

    std::unique_ptr<QCoreApplication> app; // 创建了一个智能指针

    // make_unique主要目的就是安全地创建 unique_ptr
    if (useHeadlessApplication)
    {
        app =
            std::make_unique<QCoreApplication>(
                argc,
                argv);
    }
    else
    {
        app =
            std::make_unique<QApplication>(
                argc,
                argv);
    }

    QCoreApplication::setApplicationName(
        "ADASim");

    QCoreApplication::setApplicationVersion(
        ADASIM_VERSION);

    // ==========================================
    // 创建命令行解析器
    // ==========================================

    QCommandLineParser parser;

    QCommandLineOption testOption(
        QStringList() << "test",
        "Enable automatic test mode");

    parser.addOption(testOption);

    parser.setApplicationDescription(
        "ADASim autonomous driving simulator");

    parser.addHelpOption();

    parser.addVersionOption();

    // ==========================================
    // --config
    // ==========================================

    QString defaultConfigPath =
        QCoreApplication::applicationDirPath() +
        "/config/adasim.ini";

    QCommandLineOption configOption(
        QStringList()
            << "c"
            << "config",
        "ADASim config file",
        "file",
        defaultConfigPath);

    parser.addOption(
        configOption);

    // ==========================================
    // V2.4：正式注册 --headless
    // ==========================================

    QCommandLineOption headlessOption(
        QStringList()
            << "headless",
        "Run ADASim without GUI");

    parser.addOption(
        headlessOption);

    QString scenarioPath;

    QCommandLineOption scenarioOption(
        QStringList()
            << "s"
            << "scenario",
        "Scenario json file",
        "file");

    parser.addOption(
        scenarioOption);

    QCommandLineOption suiteOption(
        QStringList() << "suite",
        "Run a test suite",
        "file");
    parser.addOption(suiteOption);

    // 正式解析命令行
    parser.process(
        *app);

    // 必须先解析，才能知道用户有没有传 --test。
    const bool testMode = parser.isSet(testOption);

    if (parser.isSet(scenarioOption))
    {
        scenarioPath = parser.value(scenarioOption);
    }

    QString configPath = parser.value(configOption);

    bool headlessMode = parser.isSet(headlessOption);

    const bool suiteMode = parser.isSet(suiteOption);
    const QString suitePath = parser.value(suiteOption);

    if (suiteMode && parser.isSet(scenarioOption))
    {
        qCritical() << "--suite cannot be used with --scenario";
        return 2;
    }

    QString dataPath;

    // ==========================================
    // Linux SIGINT / SIGTERM
    // ==========================================

    LinuxSignalHandler signalHandler;

    signalHandler.install();

    // ==========================================
    // Headless模式
    // ==========================================

    if (headlessMode)
    {
        if (suiteMode)
        {
            TestSuiteRunner suiteRunner(configPath);
            return suiteRunner.run(suitePath);
        }

        HeadlessRunner runner(
            configPath,
            dataPath,
            scenarioPath);

        QObject::connect(
            &signalHandler,
            &LinuxSignalHandler::terminationRequested,
            &runner,
            &HeadlessRunner::onTerminationRequested);

        runner.setTestMode(testMode);

        if (!runner.initialize())
        {
            qCritical() << "ADASim initialization failed";
            return 2;
        }

        runner.start();
        return app->exec();
    }

    // ==========================================
    // GUI模式
    // ==========================================

    MainWindow mainWindow(
        configPath,
        dataPath);

    QObject::connect(
        &signalHandler,
        &LinuxSignalHandler::terminationRequested,
        &mainWindow,
        &MainWindow::onTerminationRequested);

    mainWindow.showFullScreen();

    return app->exec();
}