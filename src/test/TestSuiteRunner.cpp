#include "TestSuiteRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QTextStream>
#include <QtGlobal>
#include <QDebug>

TestSuiteRunner::TestSuiteRunner(
    const QString &configPath)
    : configPath_(configPath)
{
}

int TestSuiteRunner::run(const QString &suitePath)
{
    QString errorMessage;
    if (!suite_.load(suitePath, errorMessage))
    {
        qCritical().noquote() << "[SUITE]" << errorMessage;
        return 2;
    }

    qInfo().noquote()
        << QString("[SUITE] Loaded: %1 cases=%2")
               .arg(suite_.name())
               .arg(suite_.cases().size());

    for (const TestCase &testCase : suite_.cases())
    {
        const TestRunResult result = runSingleScenario(testCase);
        results_.append(result);

        const char *tag =
            (result.exitCode == 0) ? "PASS" : (result.exitCode == 1) ? "FAIL"
                                                                     : "ERROR";

        qInfo().noquote()
            << QString("[SUITE] [%1] %2 (exit=%3)")
                   .arg(tag)
                   .arg(result.name)
                   .arg(result.exitCode);
    }

    if (!saveSummaryReport())
    {
        qCritical() << "[SUITE] Failed to write summary report";
        return 2;
    }

    const int exitCode = suiteExitCode();
    qInfo().noquote()
        << QString("[SUITE] Finished with exit code %1")
               .arg(exitCode);
    return exitCode;
}

TestRunResult TestSuiteRunner::runSingleScenario(
    const TestCase &testCase)
{
    TestRunResult result;
    result.name = testCase.name;

    const QString program =
        QCoreApplication::applicationFilePath();

    QStringList args;
    args << "--headless"
         << "--test"
         << "--scenario"
         << testCase.scenarioPath;

    if (!configPath_.isEmpty())
    {
        args << "--config" << configPath_;
    }

    QProcess process;
    process.setWorkingDirectory(QDir::currentPath());
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(program, args);

    if (!process.waitForStarted())
    {
        qCritical().noquote()
            << "[SUITE] Failed to start case:"
            << testCase.name;
        result.exitCode = 2;
        return result;
    }

    const int timeoutMs = 5 * 60 * 1000;
    if (!process.waitForFinished(timeoutMs))
    {
        qCritical().noquote()
            << "[SUITE] Case timed out:"
            << testCase.name;
        process.kill();
        process.waitForFinished(5000);
        result.exitCode = 2;
        return result;
    }

    result.exitCode = process.exitCode();
    result.passed = (result.exitCode == 0);

    const QString caseDirPath =
        QString("test_result/%1").arg(testCase.name);
    if (!QDir().mkpath(caseDirPath))
    {
        qWarning().noquote()
            << "[SUITE] Cannot create case report directory:"
            << caseDirPath;
        return result;
    }

    const QString sourcePath = QStringLiteral("test_result/report.txt");
    const QString destPath = caseDirPath + "/report.txt";
    QFile::remove(destPath);
    if (QFile::exists(sourcePath))
    {
        if (!QFile::copy(sourcePath, destPath))
        {
            qWarning().noquote()
                << "[SUITE] Cannot copy report for case:"
                << testCase.name;
        }
        else
        {
            result.reportPath = destPath;
        }
    }

    return result;
}

bool TestSuiteRunner::saveSummaryReport() const
{
    if (!QDir().mkpath("test_result"))
        return false;

    QSaveFile file("test_result/summary.txt");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    int passed = 0;
    int failed = 0;
    int errors = 0;

    for (const TestRunResult &item : results_)
    {
        if (item.exitCode == 0)
            ++passed;
        else if (item.exitCode == 1)
            ++failed;
        else
            ++errors;
    }

    QTextStream out(&file);
    out << "ADASim Regression Report\n\n"
        << "Total: " << results_.size() << "\n"
        << "Passed: " << passed << "\n"
        << "Failed: " << failed << "\n"
        << "Errors: " << errors << "\n\n";

    for (const TestRunResult &item : results_)
    {
        const char *tag =
            (item.exitCode == 0) ? "PASS" : (item.exitCode == 1) ? "FAIL"
                                                                 : "ERROR";
        out << "[" << tag << "] " << item.name << "\n";
    }

    out.flush();
    if (out.status() != QTextStream::Ok)
    {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

int TestSuiteRunner::suiteExitCode() const
{
    bool hasFail = false;

    for (const TestRunResult &item : results_)
    {
        if (item.exitCode != 0 && item.exitCode != 1)
            return 2;
        if (item.exitCode == 1)
            hasFail = true;
    }

    return hasFail ? 1 : 0;
}