#include "TestSuite.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

const QString &TestSuite::name() const
{
    return name_;
}

const QVector<TestCase> &TestSuite::cases() const
{
    return cases_;
}

bool TestSuite::load(
    const QString &filePath,
    QString &errorMessage)
{
    const QFileInfo fileInfo(filePath);

    if (!fileInfo.exists())
    {
        errorMessage =
            QString("Test suite file not found: %1")
                .arg(filePath);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage =
            QString("Failed to open test suite file: %1")
                .arg(filePath);
        return false;
    }

    const QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        errorMessage =
            QString("Test suite JSON parse failed: %1")
                .arg(parseError.errorString());
        return false;
    }

    if (!document.isObject())
    {
        errorMessage = "Test suite root must be a JSON object";
        return false;
    }

    const QJsonObject root = document.object();

    const QJsonValue nameValue = root.value("name");
    if (!nameValue.isString())
    {
        errorMessage = "Test suite missing valid string field: name";
        return false;
    }

    const QString suiteName = nameValue.toString().trimmed();
    if (suiteName.isEmpty())
    {
        errorMessage = "Test suite name cannot be empty";
        return false;
    }

    const QJsonValue casesValue = root.value("cases");
    if (!casesValue.isArray())
    {
        errorMessage = "Test suite missing valid array field: cases";
        return false;
    }

    const QJsonArray caseArray = casesValue.toArray();
    if (caseArray.isEmpty())
    {
        errorMessage = "Test suite field 'cases' cannot be empty";
        return false;
    }

    const QDir suiteDir = fileInfo.absoluteDir();
    QVector<TestCase> loadedCases;
    QSet<QString> usedNames;

    for (int i = 0; i < caseArray.size(); ++i)
    {
        const QJsonValue caseValue = caseArray.at(i);
        if (!caseValue.isObject())
        {
            errorMessage =
                QString("Test case at index %1 must be a JSON object")
                    .arg(i);
            return false;
        }

        const QJsonObject caseObject = caseValue.toObject();

        const QJsonValue caseNameValue = caseObject.value("name");
        if (!caseNameValue.isString())
        {
            errorMessage =
                QString("Test case at index %1 missing valid string field: name")
                    .arg(i);
            return false;
        }

        const QString caseName = caseNameValue.toString().trimmed();
        if (caseName.isEmpty())
        {
            errorMessage =
                QString("Test case at index %1 name cannot be empty")
                    .arg(i);
            return false;
        }

        if (usedNames.contains(caseName))
        {
            errorMessage =
                QString("Duplicate test case name: %1")
                    .arg(caseName);
            return false;
        }

        const QJsonValue scenarioValue = caseObject.value("scenario");
        if (!scenarioValue.isString())
        {
            errorMessage =
                QString("Test case at index %1 missing valid string field: scenario")
                    .arg(i);
            return false;
        }

        const QString scenarioText = scenarioValue.toString().trimmed();
        if (scenarioText.isEmpty())
        {
            errorMessage =
                QString("Test case at index %1 scenario cannot be empty")
                    .arg(i);
            return false;
        }

        const QString scenarioPath =
            QFileInfo(scenarioText).isAbsolute()
                ? QFileInfo(scenarioText).absoluteFilePath()
                : QFileInfo(suiteDir, scenarioText).absoluteFilePath();

        if (!QFileInfo::exists(scenarioPath))
        {
            errorMessage =
                QString("Test case '%1' scenario file not found: %2")
                    .arg(caseName, scenarioPath);
            return false;
        }

        TestCase testCase;
        testCase.name = caseName;
        testCase.scenarioPath = scenarioPath;
        loadedCases.append(testCase);
        usedNames.insert(caseName);
    }

    name_ = suiteName;
    cases_ = loadedCases;
    errorMessage.clear();
    return true;
}