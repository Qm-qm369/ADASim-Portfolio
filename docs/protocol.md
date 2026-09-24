# C++ 与 Python 通信协议

本文说明 SocketServer、PlannerLink 和 Python Planner 之间的消息约定。

协议版本为 `1`。

协议版本与软件发布版本是两个独立概念。

## 1. 连接方式

| 项目 | 约定 |
|---|---|
| 传输 | TCP |
| C++ 角色 | Server |
| Python 角色 | Client |
| 默认地址 | `127.0.0.1` |
| 默认端口 | `8080` |
| 内容 | UTF-8 JSON |
| 分帧 | 每条消息以 LF，即 `\n` 结束 |
| 客户端数量 | 同时一个 |

端口由 C++ 的 INI 配置决定，Python 端必须使用相同端口。

当前服务面向本机通信，不包含认证或加密机制。

## 2. 消息分帧

TCP 提供字节流，不保证一次读取对应一条消息。

接收方应：

1. 缓存未完成的消息。
2. 按换行符提取完整消息。
3. 分别处理一次读取中的多条消息。
4. 保留不足一行的剩余数据。

下面的 JSON 为方便阅读使用多行展示；实际发送时应序列化成单行，并追加一个换行符。

单行示例：

```text
{"type":"CONTROL","protocol_version":1,"session_id":"example-session","frame_id":1,"steer_offset":1.75}\n
```

其中 `\n` 表示实际 LF 字节，不是反斜杠和字母 n 两个普通字符。

## 3. 障碍物请求：OBSTACLES

方向：C++ → Python。

```json
{
  "type": "OBSTACLES",
  "protocol_version": 1,
  "session_id": "2d240c88-6ff2-4ac1-93bc-9ed78d510614",
  "frame_id": 1,
  "data": [
    {
      "local_x": 20.0,
      "local_y": 0.0,
      "dist": 20.0
    }
  ]
}
```

| 字段 | 类型 | 含义 |
|---|---|---|
| `type` | string | 固定为 `OBSTACLES` |
| `protocol_version` | integer | 当前为 `1` |
| `session_id` | string | C++ 生成的当前会话标识 |
| `frame_id` | integer | 当前会话中的请求编号 |
| `data` | array | 障碍物列表，可以为空 |
| `local_x` | number | 障碍物在自车局部坐标中的纵向位置，单位 m |
| `local_y` | number | 障碍物在自车局部坐标中的横向位置，单位 m |
| `dist` | number | 障碍物距离，单位 m |

`local_x > 0` 表示位于车辆前方。

Python 请求校验要求障碍物数值字段为有限数值。

## 4. 控制回复：CONTROL

方向：Python → C++。

```json
{
  "type": "CONTROL",
  "protocol_version": 1,
  "session_id": "2d240c88-6ff2-4ac1-93bc-9ed78d510614",
  "frame_id": 1,
  "steer_offset": 1.75
}
```

| 字段 | 类型 | 含义 |
|---|---|---|
| `type` | string | 固定为 `CONTROL` |
| `protocol_version` | integer | 当前为 `1` |
| `session_id` | string | 原样回传对应请求的会话标识 |
| `frame_id` | integer | 原样回传对应请求的帧编号 |
| `steer_offset` | number | 规划器输出的横向目标偏移，单位 m |

虽然字段名为 `steer_offset`，它表示横向位置偏移，不表示转向角。

当前 C++ 接受的偏移范围为：

```text
-3.5 ≤ steer_offset ≤ 3.5
```

数值必须有限，不能使用 NaN、Infinity 或字符串形式的数字。

## 5. 会话管理

PlannerLink 在以下情况下更换会话标识并清空待处理请求：

- 仿真运行标识变化。
- Planner 连接建立。
- Planner 连接断开。
- 帧编号达到上限，需要重新开始编号。

帧编号从 `1` 开始递增，上限为 `2147483647`。

Python 必须回传收到的会话标识，不能自行生成。

会话校验用于拒绝旧连接或旧运行周期的迟到回复。

## 6. 顺序校验

CONTROL 回复需要同时满足：

- 当前处于已连接且有有效仿真运行标识的状态。
- 协议版本与消息类型正确。
- 会话标识与当前会话一致。
- 帧编号是有效的正 int32。
- 帧编号存在于待处理请求中。
- 帧编号大于最近一次已接受的编号。
- 回复没有过期。
- 横向偏移合法。

例如，编号 11 和 12 均处于待处理状态，若先接受 12，再收到 11，后者将被拒绝。

接受某个编号后，小于或等于它的待处理请求会被移除。

## 7. 时效校验

当前最大有效年龄：

```text
500 ms
```

当年龄大于或等于 500 ms 时，请求或回复被视为过期。

计时起点是 C++ 产生对应仿真感知输入时记录的单调时间。因此校验包含：

- C++ 内部排队与处理。
- 网络传输。
- Python 计算。
- 回复返回后的处理。

它不是只测量 TCP 往返时间。

时间戳由 C++ 本地保存，不要求 Python 与 C++ 同步系统时钟，也没有在当前消息中传输该时间戳。

## 8. 资源限制

| 限制 | 当前值 |
|---|---:|
| 单行消息最大长度，不含末尾 LF | 65,536 字节 |
| C++ 待发送数据上限 | 262,144 字节 |
| PlannerLink 待处理请求数量上限 | 64 |
| C++ 单轮读取处理行数上限 | 64 |

待处理请求数量达到上限时，会淘汰最早的未完成请求。

单轮读取上限用于将执行机会交还 Qt 事件循环，剩余完整行安排后续处理。

这些限制描述当前 C++ 实现，不代表 Python 端具有相同的缓冲上限。

## 9. 错误处理

### 传输层错误

以下情况会导致连接关闭或拒绝：

- 超长消息。
- 无换行的超长输入。
- 非法发送分帧。
- 发送积压超过上限。
- TCP 错误。
- 已有客户端时接入第二个客户端。

第二个客户端被拒绝时，原客户端继续保留。

### 协议错误

非法 JSON、错误字段、旧会话、重复、乱序或过期回复由 PlannerLink 拒绝，并产生本地错误通知。

当前没有定义向 Python 返回的 ERROR 消息或确认消息。

拒绝一条回复并不等于已实现自动制动、自动重连或完整故障降级策略。

## 10. 实现入口

- [SocketServer](../src/communication/Socket.cpp)：连接、分帧与传输限制。
- [PlannerLink](../src/communication/PlannerLink.cpp)：字段、会话、顺序和时效校验。
- [PlannerLink 常量](../src/communication/PlannerLink.h)：有效年龄与待处理数量。
- [Python Planner](../tools/python_planner.py)：请求解析与回复构造。

## 11. 验证边界

SocketServer 和 PlannerLink 已有针对部分行为的自动化测试。

当前 Python 客户端存在待修复问题，且 C++ 接收偏移后的轨迹更新链路需要验证。因此协议测试通过不能替代 C++ / Python 端到端避障验证。

测试方法见 [测试说明](testing.md)。