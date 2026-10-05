#ifndef SCENARIOLOADER_H
#define SCENARIOLOADER_H

#include <QString>
#include <QVector>
#include <QPointF>

#include "TestEvaluator.h"

struct ScenarioTestConfig
{
    int maxFrames = 200;

    AebExpectation aebExpectation =
        AebExpectation::Any;
};

// V2.5 新增：
// 表示已经从 JSON 文件解析完成的一份仿真场景。
struct Scenario
{
    // 场景名称
    QString name;

    // 静态障碍物的世界坐标
    QVector<QPointF> obstacles;

    ScenarioTestConfig test;
};

class ScenarioLoader
{
public:
    // 从 JSON 文件读取场景。
    //
    // 成功：
    //   返回 true
    //   scenario 保存解析后的场景
    //
    // 失败：
    //   返回 false
    //   errorMessage 保存具体错误原因
    static bool load(
        const QString &filePath,
        Scenario &scenario,
        QString &errorMessage);
};

#endif