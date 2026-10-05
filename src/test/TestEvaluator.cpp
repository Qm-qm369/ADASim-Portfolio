#include "TestEvaluator.h"

TestEvaluator::TestEvaluator(
    QObject *parent)
    : QObject(parent)
{
}

void TestEvaluator::reset()
{
    result_ = TestResult();
}

void TestEvaluator::processFrame(
    const SimulationFrame &frame)
{

    ++result_.frameCount;

    if (frame.emergencyBrake)
    {
        result_.aebTriggered = true;
    }

    if (frame.ttc > 0)
    {

        if (result_.minTtc < 0 ||
            frame.ttc < result_.minTtc)
        {
            result_.minTtc =
                frame.ttc;
        }
    }

    /*
        注意：
        frontObstacleDistance=-1
        表示没有障碍物

        不能认为碰撞
    */

    if (frame.frontObstacleDistance > 0 &&
        frame.frontObstacleDistance < 1.0)
    {
        result_.collision = true;
    }
}

TestResult TestEvaluator::result(
    AebExpectation expectation) const
{
    TestResult result = result_;

    bool aebMatches = true;

    if (expectation == AebExpectation::Required)
        aebMatches = result.aebTriggered;
    else if (expectation == AebExpectation::Forbidden)
        aebMatches = !result.aebTriggered;

    result.passed =
        result.frameCount > 0 &&
        !result.collision &&
        aebMatches;

    return result;
}