# 测试说明

ADASim 包含组件测试、headless smoke 测试和场景回归运行功能。

RK3568 和 PTY 串口的手动验收见 [板端部署与测试](rk3568-deployment.md)。当前五个 CTest 条目不覆盖新增串口模块；自动场景测试应关闭串口并断开外部 Planner，避免控制输入影响结果。

本文描述已有测试入口与判断规则，不将仓库中历史报告视为当前版本的验证结果。

## 1. 测试分层

| 层级 | 入口 | 目的 |
|---|---|---|
| 组件测试 | Qt Test | 验证评估器、场景解析和通信组件 |
| Smoke 测试 | CTest + Bash | 验证无界面程序能完成指定场景并生成报告 |
| 场景回归 | `--headless --suite` | 顺序执行多个场景并汇总结果 |
| 手动集成验证 | GUI + Python Planner | 检查跨进程通信和实际车辆响应 |

当前自动化测试不包含完整 GUI 交互与 Python Planner 端到端避障测试。

## 2. 构建与执行

在仓库根目录执行：

```bash
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON

cmake --build build-test --parallel

ctest --test-dir build-test --output-on-failure
```

查看已注册的 CTest 条目：

```bash
ctest --test-dir build-test -N
```

运行单个测试：

```bash
ctest --test-dir build-test \
  -R '^test_planner_link$' \
  --output-on-failure
```

当前注册五个 CTest 条目。一个 Qt Test 可执行文件内部可能包含多个测试函数，不能将 CTest 条目数直接当作所有测试用例数量。

## 3. 已有测试范围

| CTest 名称 | 主要覆盖范围 |
|---|---|
| `test_test_evaluator` | 无碰撞通过、碰撞失败、要求 AEB 但未触发时失败 |
| `test_scenario_loader` | 场景解析、测试配置、缺省配置、非法配置和损坏 JSON |
| `test_socket_server` | 分片与粘连消息、长度边界、超长输入、重连后残留数据隔离、第二客户端拒绝 |
| `test_planner_link` | 有效回复、重复与乱序、非法字段、年龄边界和会话边界 |
| `headless_empty_scenario` | headless 空场景运行、退出码和报告生成 |

完整测试定义见 [tests/CMakeLists.txt](../tests/CMakeLists.txt)。

测试存在不代表该功能的所有边界均已覆盖。

## 4. Headless smoke 测试

脚本：

```text
tests/run_headless_smoke.sh
```

CTest 为脚本传入：

- ADASim 可执行文件。
- `tests/fixtures/adasim_smoke.ini`。
- `scenarios/empty.json`。

该测试在临时目录运行，检查：

1. 程序退出码为 0。
2. `test_result/report.txt` 存在。
3. 报告含有 `PASS:` 字段。

脚本不独立解析 PASS 字段的真假，成功判断主要依赖程序退出码。

当前 smoke 配置使用端口 `18080`，应确保端口未被占用。测试结束后临时工作目录被清理。

## 5. 单场景测试

运行：

```bash
./build-test/ADASim \
  --headless \
  --test \
  --config config/adasim.ini \
  --scenario scenarios/empty.json

result=$?
printf 'exit code: %s\n' "$result"
```

自动测试时不要连接外部 Python Planner，以免改变场景原本要验证的控制行为。

当前示例场景：

| 场景 | 最大帧数 | AEB 期望 |
|---|---:|---|
| `empty.json` | 100 | Forbidden |
| `straight_obstacle.json` | 200 | Required |
| `multi_obstacle.json` | 300 | Required |

DataLoader 当前按 100 ms 周期产生 Tick，运行时间受事件循环调度影响。这些测试不是无限速离线仿真。

## 6. 测试配置

场景可包含：

```json
{
  "test": {
    "max_frames": 200,
    "aeb_expectation": "Required"
  }
}
```

建议 `max_frames` 使用 `1` 至 `1000000` 范围内的整数。

未提供测试配置时，默认运行 200 帧，AEB 期望为 Any。

| AEB 期望 | 条件 |
|---|---|
| `Any` | 是否触发均可 |
| `Required` | 必须至少触发一次 |
| `Forbidden` | 全程不得触发 |

这些期望由场景定义，目前不能用独立命令行参数覆盖。

## 7. 通过条件与局限

TestEvaluator 当前通过条件：

- 至少处理一帧。
- 未命中简化碰撞判据。
- AEB 实际行为满足期望。

当前碰撞判据为：

```text
0 < frontObstacleDistance < 1.0 m
```

该判据不是完整几何碰撞检测，不能用于证明任意场景下的碰撞安全性。

评估器记录最小正 TTC，但当前未将某个 TTC 阈值作为独立通过条件。没有正 TTC 样本时，值保持初始的 `-1`。

此外：

- 当前评分不直接约束横向跟踪误差或速度误差。
- 当前评分不证明 Python Planner 成功参与控制。
- 测试运行帧数由 HeadlessRunner 控制，评估器自身只检查帧数大于零。

## 8. 退出码

对于 headless 自动测试：

| 退出码 | 含义 |
|---|---|
| `0` | 评估通过，且报告保存成功 |
| `1` | 运行完成，但评估未通过 |
| `2` | 初始化、报告写入等运行错误，或测试被中断 |

初始化失败或中途退出时，报告可能不存在。检查结果应同时结合退出码、日志和本次产生的文件。

## 9. 单场景报告

路径：

```text
test_result/report.txt
```

当前报告包含：

- Scenario。
- AEB Expectation。
- Max Frames。
- Actual Frames。
- PASS。
- AEB。
- Collision。
- Min TTC。

布尔字段通过 QTextStream 输出，通常表示为 `0` 或 `1`。

报告相对于启动工作目录生成，重复运行会覆盖同一路径。

## 10. 批量回归

运行仓库中的套件：

```bash
./build-test/ADASim \
  --headless \
  --config config/adasim.ini \
  --suite scenarios/regression.json

result=$?
printf 'suite exit code: %s\n' "$result"
```

套件文件结构：

```json
{
  "name": "ADASim regression",
  "cases": [
    {
      "name": "empty",
      "scenario": "empty.json"
    },
    {
      "name": "straight_obstacle",
      "scenario": "straight_obstacle.json"
    }
  ]
}
```

上例按套件文件位于 `scenarios/` 编写。相对场景路径以套件文件所在目录为基准解析。

TestSuiteRunner 顺序启动子进程，各子进程使用 `--headless --test` 运行。当前单场景等待超时为 5 分钟。

输出结构：

```text
test_result/
├── report.txt
├── summary.txt
├── empty/
│   └── report.txt
├── straight_obstacle/
│   └── report.txt
└── multi_obstacle/
    └── report.txt
```

套件退出码：

- 所有场景通过：`0`。
- 存在评估失败，且没有运行错误：`1`。
- 存在运行错误或汇总报告写入失败：`2`。

当前套件复用工作目录中的 `test_result/report.txt` 再复制到场景目录。子进程异常退出时存在误用旧报告的风险，因此异常场景的报告副本不能单独作为本次运行证据。

当前场景报告复制失败也不一定改变该场景退出码，需要结合日志核查。

## 11. GitHub Actions

工作流：

```text
.github/workflows/build-and-test.yml
```

当前配置：

1. 在 push 和 pull request 时触发。
2. 使用 Ubuntu 22.04。
3. 安装 Qt 5 与构建工具。
4. 启用 BUILD_TESTING。
5. 编译项目。
6. 执行 CTest。

当前工作流没有单独执行 `--suite scenarios/regression.json`，也没有执行 Python Planner 集成验证。

因此，Actions 成功表示其配置中的构建与测试成功，不代表所有示例场景和跨语言链路均已通过。

## 12. Python 与端到端验证

当前 Python Planner 存在已知错误，修复内容见 [使用说明](usage.md)。

修复后，先检查语法：

```bash
python3 -m py_compile tools/python_planner.py
```

语法检查通过后，还需手动验证：

1. Python 能连接 C++。
2. Python 收到 OBSTACLES 并返回 CONTROL。
3. 会话和帧编号原样回传。
4. 合法偏移进入 C++ 控制入口。
5. 参考轨迹与车辆响应按预期变化。
6. 重复、过期和旧会话回复被拒绝。

语法检查无法发现无条件跳过规划等业务逻辑问题。

当前 `onLateralControlReceived()` 未接入完整轨迹更新流程，修复并验证前，不应宣称完整避障闭环已通过。

## 13. 保存验证证据

每次用于发布或简历展示的验证，建议记录：

- Git 提交 SHA。
- 操作系统。
- 编译器、CMake、Qt 与 Python 版本。
- 构建类型。
- 执行命令。
- 配置文件与场景。
- 退出码。
- CTest 输出或 Actions 运行链接。
- 当次生成的报告与演示素材。

查看基本环境：

```bash
git rev-parse HEAD
cat /etc/os-release
c++ --version
cmake --version
qmake -v
python3 --version
```

历史报告、预期结果与本次实测结果应明确区分。未执行的检查不写成“已通过”。
