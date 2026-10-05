#include "SerialProtocol.h"

#include <cmath>
#include <locale>
#include <sstream>

SerialCommand parseSerialCommand(const std::string &line)
{
    std::istringstream input(line);

    // 协议统一使用英文小数点，例如 3.5。
    input.imbue(std::locale::classic());

    std::string name;

    if (!(input >> name)) {
        return {
            CommandType::Invalid,
            0.0,
            "empty-command"
        };
    }

    // PING 和 GET_STATE 不接受参数。
    if (name == "PING" || name == "GET_STATE") {
        std::string extra;

        if (input >> extra) {
            return {
                CommandType::Invalid,
                0.0,
                "unexpected-argument"
            };
        }

        return {
            name == "PING"
                ? CommandType::Ping
                : CommandType::GetState,
            0.0,
            ""
        };
    }

    // SET_SPEED 必须提供一个数值参数。
    if (name == "SET_SPEED") {
        double speed = 0.0;

        if (!(input >> speed)) {
            return {
                CommandType::Invalid,
                0.0,
                "invalid-speed"
            };
        }

        // 拒绝多余参数，也拒绝 3abc 之类的输入。
        std::string extra;

        if (input >> extra) {
            return {
                CommandType::Invalid,
                0.0,
                "unexpected-argument"
            };
        }

        // 本练习规定允许范围为 0～30 m/s。
        if (!std::isfinite(speed) ||
            speed < 0.0 ||
            speed > 30.0) {
            return {
                CommandType::Invalid,
                0.0,
                "speed-out-of-range"
            };
        }

        return {
            CommandType::SetSpeed,
            speed,
            ""
        };
    }

    return {
        CommandType::Invalid,
        0.0,
        "unknown-command"
    };
}
