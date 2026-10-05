#ifndef TESTSUITE_H
#define TESTSUITE_H

#include <QString>
#include <QVector>

struct TestCase
{
    QString name;
    QString scenarioPath;
};

class TestSuite
{
public:
    bool load(const QString &filePath, QString &errorMessage);

    const QString &name() const;
    const QVector<TestCase> &cases() const;

private:
    QString name_;
    QVector<TestCase> cases_;
};

#endif