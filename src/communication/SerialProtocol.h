#pragma once

#include <string>

// 解析后得到的命令类型。
enum class CommandType
{
    Ping,
    SetSpeed,
    GetState,
    Invalid
};

// 保存命令类型、参数或者错误原因。
struct SerialCommand
{
    CommandType type = CommandType::Invalid;
    double speedMps = 0.0;
    std::string error;
};

// 输入不包含结尾换行的一行文本，返回解析结果。
SerialCommand parseSerialCommand(const std::string &line);
