#include <QtTest>

#include "scenario/ScenarioLoader.h"

class ScenarioLoaderTest : public QObject
{
    Q_OBJECT

private:
    static QString fixture(const char *fileName)
    {
        const QString path = QFINDTESTDATA(fileName);
        if (path.isEmpty())
        {
            qWarning("Missing fixture: %s", fileName);
        }
        return path;
    }

private slots:
    void loadsEmptyScene();
    void loadsObstacleScene();
    void loadsTestConfig();
    void usesDefaultTestConfigWhenMissing();
    void rejectsInvalidFrames();
    void rejectsInvalidExpectation();
    void rejectsBrokenJson();
};

void ScenarioLoaderTest::loadsEmptyScene()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/empty.json"),
        scenario,
        errorMessage);

    QVERIFY2(success, qPrintable(errorMessage));
    QCOMPARE(scenario.name, QString("empty"));
    QCOMPARE(scenario.obstacles.size(), 0);
}

void ScenarioLoaderTest::loadsObstacleScene()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/obstacles.json"),
        scenario,
        errorMessage);

    QVERIFY2(success, qPrintable(errorMessage));
    QCOMPARE(scenario.name, QString("obstacles"));
    QCOMPARE(scenario.obstacles.size(), 2);
    QCOMPARE(scenario.obstacles[0].x(), 15.0);
    QCOMPARE(scenario.obstacles[0].y(), 0.0);
    QCOMPARE(scenario.obstacles[1].x(), 25.0);
    QCOMPARE(scenario.obstacles[1].y(), 1.5);
}

void ScenarioLoaderTest::loadsTestConfig()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/required_aeb.json"),
        scenario,
        errorMessage);

    QVERIFY2(success, qPrintable(errorMessage));
    QCOMPARE(scenario.test.maxFrames, 200);
    QCOMPARE(scenario.test.aebExpectation, AebExpectation::Required);
}

void ScenarioLoaderTest::usesDefaultTestConfigWhenMissing()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/empty.json"),
        scenario,
        errorMessage);

    QVERIFY2(success, qPrintable(errorMessage));
    QCOMPARE(scenario.test.maxFrames, 200);
    QCOMPARE(scenario.test.aebExpectation, AebExpectation::Any);
}

void ScenarioLoaderTest::rejectsInvalidFrames()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/invalid_max_frames.json"),
        scenario,
        errorMessage);

    QVERIFY(!success);
    QVERIFY(!errorMessage.isEmpty());
}

void ScenarioLoaderTest::rejectsInvalidExpectation()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/invalid_expectation.json"),
        scenario,
        errorMessage);

    QVERIFY(!success);
    QVERIFY(!errorMessage.isEmpty());
}

void ScenarioLoaderTest::rejectsBrokenJson()
{
    Scenario scenario;
    QString errorMessage;

    const bool success = ScenarioLoader::load(
        fixture("fixtures/invalid_json.json"),
        scenario,
        errorMessage);

    QVERIFY(!success);
    QVERIFY(!errorMessage.isEmpty());
}

QTEST_MAIN(ScenarioLoaderTest)
#include "test_scenario_loader.moc"