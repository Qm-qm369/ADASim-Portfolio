#ifndef TESTSUITERUNNER_H
#define TESTSUITERUNNER_H

#include <QString>
#include <QVector>

#include "TestSuite.h"

struct TestRunResult
{
    QString name;
    int exitCode = 2;
    bool passed = false;
    QString reportPath;
};

class TestSuiteRunner
{
public:
    explicit TestSuiteRunner(
        const QString &configPath);

    int run(const QString &suitePath);

private:
    TestRunResult runSingleScenario(const TestCase &testCase);
    bool saveSummaryReport() const;
    int suiteExitCode() const;

    QString configPath_;
    TestSuite suite_;
    QVector<TestRunResult> results_;
};

#endif