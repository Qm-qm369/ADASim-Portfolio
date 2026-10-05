# 使用说明

本文介绍 Linux 环境下的构建、启动、配置与场景运行。

RK3568 交叉编译、打包部署及 `[serial]` 配置见 [板端部署与测试](rk3568-deployment.md)。串口只在 headless 中启用；GUI 不处理串口命令。

除特别说明外，所有命令均在仓库根目录执行。

## 1. 环境与依赖

仓库 CI 配置使用 Ubuntu 22.04 和 Qt 5。

主要依赖：

- 支持 C++17 的编译器。
- CMake 3.16 或更新版本。
- Qt Core、Gui、Widgets、Network、Test。
- Python 3。
- Bash，用于 headless smoke 测试。

CMake 支持查找 Qt 5 或 Qt 6，但当前 CI 配置不代表所有 Qt 版本均已验证。

Ubuntu 依赖安装：

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  qtbase5-dev \
  qtbase5-dev-tools \
  python3
```

## 2. 构建

Debug 构建并启用自动化测试：

```bash
cmake -S . -B build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON

cmake --build build-debug --parallel
```

查看命令行帮助与编译版本：

```bash
./build-debug/ADASim --headless --help
./build-debug/ADASim --headless --version
```

这里使用 `--headless`，避免在没有图形桌面的环境中创建 QApplication。

版本字符串由 CMake 项目版本生成。

运行自动化测试：

```bash
ctest --test-dir build-debug --output-on-failure
```

如果本机 CTest 不支持 `--test-dir`，可使用：

```bash
(cd build-debug && ctest --output-on-failure)
```

## 3. GUI 模式

在有图形桌面的环境中启动：

```bash
./build-debug/ADASim --config config/adasim.ini
```

打开窗口后，通过界面提供的仿真运行和控制操作进行演示。

GUI 当前未将 `--scenario` 参数传给 MainWindow；需要加载 JSON 场景时使用 headless 模式。

## 4. Headless 模式

加载空场景运行：

```bash
./build-debug/ADASim \
  --headless \
  --config config/adasim.ini \
  --scenario scenarios/empty.json
```

普通 headless 模式不会因为场景中设置了 `max_frames` 就自动结束。

按 Ctrl+C 退出。需要按指定帧数自动结束时，增加 `--test`：

```bash
./build-debug/ADASim \
  --headless \
  --test \
  --config config/adasim.ini \
  --scenario scenarios/empty.json
```

单场景报告写入当前工作目录下的：

```text
test_result/report.txt
```

批量运行：

```bash
./build-debug/ADASim \
  --headless \
  --config config/adasim.ini \
  --suite scenarios/regression.json
```

详细测试规则见 [测试说明](testing.md)。

## 5. 命令行参数

| 参数 | 说明 |
|---|---|
| `--headless` | 使用无界面入口 |
| `-c, --config <file>` | 指定 INI 配置 |
| `-s, --scenario <file>` | 指定场景；目前由 headless 入口使用 |
| `--test` | 按测试帧数运行并输出报告，要求 `--headless` |
| `--suite <file>` | 批量执行测试场景，要求 `--headless` |
| `--help` | 查看帮助 |
| `--version` | 查看编译版本 |

约束：

- `--suite` 不能与 `--scenario` 同时使用。
- `--suite` 会为各场景启动测试子进程，不必额外指定 `--test`。
- `--test` 的帧数和 AEB 期望来自场景 JSON，目前没有独立的命令行选项覆盖它们。

## 6. 配置文件

仓库示例：

```ini
[controller]
heading_k=1
lateral_k=1.5
look_ahead=5
mode=DualError

[network]
planner_port=8080

[simulation]
planning_distance=20
target_speed=8
```

| 配置项 | 含义 |
|---|---|
| `heading_k` | DualError 航向误差增益 |
| `lateral_k` | DualError 横向误差增益 |
| `look_ahead` | 前视距离，单位 m |
| `mode` | 控制器模式，使用 `DualError` 或 `PurePursuit` |
| `planner_port` | 本机 Planner TCP 监听端口 |
| `planning_distance` | 横向过渡规划距离，单位 m |
| `target_speed` | 目标速度，单位 m/s |

默认配置路径是可执行文件所在目录下的 `config/adasim.ini`。

CMake 会在构建目录配置文件不存在时复制默认配置。修改仓库中的 INI 后，已有构建目录中的副本不会自动被覆盖。

调试时建议显式使用：

```bash
--config config/adasim.ini
```

当前配置加载不对所有字段进行完整的语义校验，应使用合理的正数距离、速度和正确的控制器名称。

## 7. 场景文件

场景示例：

```json
{
  "name": "straight_obstacle",
  "obstacles": [
    {
      "x": 20.0,
      "y": 0.0
    }
  ],
  "test": {
    "max_frames": 200,
    "aeb_expectation": "Required"
  }
}
```

说明：

- `name`：场景名称。
- `obstacles`：世界坐标下的障碍物位置，单位 m。
- `max_frames`：测试模式运行的帧数。
- `aeb_expectation`：AEB 评估条件。

AEB 条件：

- `Any`：不限制是否触发。
- `Required`：必须至少触发一次。
- `Forbidden`：不允许触发。

场景中的测试配置只在测试模式决定结束与评分行为，不会把普通仿真自动变成测试运行。

## 8. Python Planner

### 当前状态

当前仓库版本的 `tools/python_planner.py` 存在以下问题：

- `response` 字典的 `steer_offset` 项后缺少逗号。
- 请求校验后的 `continue` 位于异常处理块外，导致有效请求也跳过后续规划。
- 序列化表达式错误地再次调用 `json.dumps()` 返回的字符串。

这些问题修复之前，不能将 Python Planner 标记为可运行。

C++ 端接收偏移后如何更新轨迹，也需要完成验证，见 [系统架构](architecture.md)。

### 修复后的启动流程

先启动 C++ 程序：

```bash
./build-debug/ADASim --config config/adasim.ini
```

另开终端，在仓库根目录执行：

```bash
python3 tools/python_planner.py
```

默认连接地址：

```text
127.0.0.1:8080
```

修改 INI 中的端口后，还需同步修改 Python 脚本中的 `PORT`。

服务器同时只接受一个 Planner 客户端。当前 Python 脚本没有自动重连循环，断开后需重新启动。

## 9. 输出文件

| 路径 | 内容 |
|---|---|
| `record/simulation.csv` | headless 仿真记录 |
| `test_result/report.txt` | 单场景测试报告 |
| `test_result/<case-name>/report.txt` | 套件中的场景报告副本 |
| `test_result/summary.txt` | 套件汇总 |

这些路径相对于程序启动时的工作目录。

重复执行可能覆盖同名文件。需要保留某次实验时，应复制结果并记录配置、场景和提交版本。

构建目录、录制数据和临时测试报告不应作为普通源码提交。

## 10. 常见问题

### 无法找到 Qt

确认已安装 Qt 开发包，而不仅是 Qt 运行时。

### 无图形环境启动失败

使用 `--headless`。当前构建仍依赖 Qt Gui 和 Widgets，无界面运行不等于构建时完全不依赖这些模块。

### TCP 监听失败

检查配置端口是否已被其他 ADASim 实例或程序占用。

### 修改配置后没有效果

显式指定仓库中的配置路径，避免读取构建目录中的旧副本。

### 测试报告未生成

查看退出码和终端错误。初始化失败或中途终止时，不应假定报告一定存在，也不要将旧报告当作本次结果。
