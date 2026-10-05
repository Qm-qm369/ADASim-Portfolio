#ifndef TESTEVALUATOR_H
#define TESTEVALUATOR_H

#include <QObject>

#include "backend/SimulationRecorder.h"

enum class AebExpectation
{
    Any,      // 不限制是否触发 AEB
    Required, // 必须触发 AEB
    Forbidden // 不允许触发 AEB
};

struct TestResult
{
    bool aebTriggered = false;

    bool collision = false;

    double minTtc = -1.0;

    bool passed = false;

    int frameCount = 0;
};

class TestEvaluator : public QObject
{
    Q_OBJECT

public:
    explicit TestEvaluator(
        QObject *parent = nullptr);

    void reset();

    void processFrame(
        const SimulationFrame &frame);

    TestResult result(
        AebExpectation expectation = AebExpectation::Any) const;

private:
    TestResult result_;
};

#endif