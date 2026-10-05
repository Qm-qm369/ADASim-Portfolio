#include <QtTest>

#include "test/TestEvaluator.h"

class TestEvaluatorTest : public QObject
{
    Q_OBJECT

private slots:
    void noCollisionPasses();
    void collisionFails();
    void requiredAebFailsWithoutAeb();
};

void TestEvaluatorTest::noCollisionPasses()
{
    TestEvaluator evaluator;

    SimulationFrame frame;
    frame.frontObstacleDistance = -1.0;
    frame.emergencyBrake = false;
    frame.ttc = -1.0;

    evaluator.processFrame(frame);

    const TestResult result =
        evaluator.result(AebExpectation::Any);

    QCOMPARE(result.frameCount, 1);
    QCOMPARE(result.collision, false);
    QVERIFY(result.passed);
}

void TestEvaluatorTest::collisionFails()
{
    TestEvaluator evaluator;

    SimulationFrame frame;
    frame.frontObstacleDistance = 0.5;

    evaluator.processFrame(frame);

    const TestResult result =
        evaluator.result(AebExpectation::Any);

    QVERIFY(result.collision);
    QVERIFY(!result.passed);
}

void TestEvaluatorTest::requiredAebFailsWithoutAeb()
{
    TestEvaluator evaluator;

    SimulationFrame frame;
    frame.frontObstacleDistance = -1.0;
    frame.emergencyBrake = false;

    evaluator.processFrame(frame);

    const TestResult result =
        evaluator.result(AebExpectation::Required);

    QCOMPARE(result.aebTriggered, false);
    QVERIFY(!result.passed);
}

QTEST_MAIN(TestEvaluatorTest)
#include "test_test_evaluator.moc"