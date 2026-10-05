#include "ScenarioLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

/*
filePath
   ↓
文件存在吗？
   ↓
能打开吗？
   ↓
readAll()
   ↓
JSON语法正确吗？
   ↓
最外层是 {} 吗？
   ↓
name 是 string 吗？
   ↓
obstacles 是 [] 吗？
   ↓
每个 obstacle 是 {} 吗？
   ↓
x 是 number 吗？
y 是 number 吗？
   ↓
QPointF(x, y)
   ↓
Scenario
*/

// 用 static 只给本文件用，不必写进头文件。
static bool parseAebExpectation(
    const QString &value,
    AebExpectation &expectation,
    QString &errorMessage)
{
    if (value == "Any")
    {
        expectation = AebExpectation::Any;
        return true;
    }

    if (value == "Required")
    {
        expectation = AebExpectation::Required;
        return true;
    }

    if (value == "Forbidden")
    {
        expectation = AebExpectation::Forbidden;
        return true;
    }

    errorMessage =
        QString("Invalid aeb_expectation: %1")
            .arg(value);

    return false;
}

static bool validateScenario(
    const Scenario &scenario,
    QString &errorMessage)
{
    if (scenario.name.trimmed().isEmpty())
    {
        errorMessage = "Scenario name cannot be empty";
        return false;
    }

    if (scenario.test.maxFrames <= 0)
    {
        errorMessage = "Scenario max_frames must be positive";
        return false;
    }

    if (scenario.test.maxFrames > 1000000)
    {
        errorMessage = "Scenario max_frames is out of range";
        return false;
    }

    return true;
}

bool ScenarioLoader::load(
    const QString &filePath,
    Scenario &scenario,
    QString &errorMessage)
{
    // ==========================================
    // 1. 检查文件是否存在
    // ==========================================

    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists())
    {
        errorMessage =
            QString(
                "Scenario file not found: %1")
                .arg(filePath);

        return false;
    }

    // ==========================================
    // 2. 打开文件
    // ==========================================

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage =
            QString(
                "Failed to open scenario file: %1")
                .arg(filePath);

        return false;
    }

    // ==========================================
    // 3. 一次读取整个 JSON 文件
    // ==========================================

    QByteArray jsonData =
        file.readAll();

    file.close();

    // ==========================================
    // 4. 解析 JSON
    // ==========================================

    QJsonParseError parseError;

    QJsonDocument document =
        QJsonDocument::fromJson(
            jsonData,
            &parseError);

    if (parseError.error !=
        QJsonParseError::NoError)
    {
        errorMessage =
            QString(
                "Scenario JSON parse failed: %1")
                .arg(
                    parseError.errorString());

        return false;
    }

    // ==========================================
    // 5. 最外层必须是 {}
    // ==========================================

    if (!document.isObject())
    {
        errorMessage =
            "Scenario root must be a JSON object";

        return false;
    }
    /*
    把 JSON 根对象取出来。
    现在 root 代表：
    {
    "name":"test",
    "obstacles":[]
    }
    */
    QJsonObject root =
        document.object();

    // ==========================================
    // 6. 检查 name
    // ==========================================

    QJsonValue nameValue =
        root.value("name");

    if (!nameValue.isString())
    {
        errorMessage =
            "Scenario missing valid string field: name";

        return false;
    }

    QString scenarioName =
        nameValue.toString();

    if (scenarioName.isEmpty())
    {
        errorMessage =
            "Scenario name cannot be empty";

        return false;
    }

    // ==========================================
    // 7. 检查 obstacles
    // ==========================================

    QJsonValue obstaclesValue =
        root.value("obstacles");

    if (!obstaclesValue.isArray())
    {
        errorMessage =
            "Scenario missing valid array field: obstacles";

        return false;
    }

    QJsonArray obstacleArray =
        obstaclesValue.toArray();

    // 先解析到临时容器。
    // 整个 JSON 全部合法以后，
    // 再真正写进 scenario。
    QVector<QPointF> obstacles;

    // ==========================================
    // 8. 逐个检查障碍物
    // ==========================================

    for (int i = 0;
         i < obstacleArray.size();
         ++i)
    {
        QJsonValue obstacleValue =
            obstacleArray.at(i);

        if (!obstacleValue.isObject())
        {
            errorMessage =
                QString(
                    "Obstacle at index %1 "
                    "must be a JSON object")
                    .arg(i);

            return false;
        }

        QJsonObject obstacleObject =
            obstacleValue.toObject();

        QJsonValue xValue =
            obstacleObject.value("x");

        QJsonValue yValue =
            obstacleObject.value("y");

        if (!xValue.isDouble())
        {
            errorMessage =
                QString(
                    "Obstacle at index %1 "
                    "missing numeric field: x")
                    .arg(i);

            return false;
        }

        if (!yValue.isDouble())
        {
            errorMessage =
                QString(
                    "Obstacle at index %1 "
                    "missing numeric field: y")
                    .arg(i);

            return false;
        }

        double x =
            xValue.toDouble();

        double y =
            yValue.toDouble();

        obstacles.append(
            QPointF(x, y));
    }

    // ==========================================
    // 8.5 解析可选的 test 配置
    // ==========================================

    ScenarioTestConfig testConfig;

    if (root.contains("test"))
    {
        if (!root.value("test").isObject())
        {
            errorMessage =
                "Scenario field 'test' must be an object";
            return false;
        }

        const QJsonObject testObject =
            root.value("test").toObject();

        if (testObject.contains("max_frames"))
        {
            if (!testObject.value("max_frames").isDouble())
            {
                errorMessage =
                    "Scenario field 'test.max_frames' must be a number";
                return false;
            }

            const int maxFrames =
                testObject.value("max_frames").toInt();

            if (maxFrames <= 0 || maxFrames > 1000000)
            {
                errorMessage =
                    "Scenario field 'test.max_frames' is out of range";
                return false;
            }

            testConfig.maxFrames = maxFrames;
        }

        if (testObject.contains("aeb_expectation"))
        {
            if (!testObject.value("aeb_expectation").isString())
            {
                errorMessage =
                    "Scenario field 'test.aeb_expectation' must be a string";
                return false;
            }

            const QString expectationText =
                testObject.value("aeb_expectation").toString();

            if (!parseAebExpectation(
                    expectationText,
                    testConfig.aebExpectation,
                    errorMessage))
            {
                return false;
            }
        }
    }

    // ==========================================
    // 9. 所有检查通过以后再更新输出
    // ==========================================

    Scenario loaded;
    loaded.name = scenarioName;
    loaded.obstacles = obstacles;
    loaded.test = testConfig;

    if (!validateScenario(loaded, errorMessage))
        return false;

    scenario = loaded;
    errorMessage.clear();
    return true;
}
