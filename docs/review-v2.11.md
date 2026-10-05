# V2.11 源码与文档检查记录

检查日期：2026-10-05。基准提交：`5f2fb1965c3965b2ae08b11ddbe3980cea7892e7`。

## 检查范围与结论

已阅读构建文件、工具链、SerialPort、SerialProtocol、HeadlessRunner、配置读写、测试注册、CI 和现有文档。串口接入与此前学习步骤一致：QSocketNotifier 非阻塞收发，按行分帧，参数检查后设置引擎目标，查询最新仿真帧，退出时关闭设备。

本次是静态审阅及文档补充，没有在本机重新执行 Linux/ARM 编译、板端测试或 GitHub Actions。用户已反馈相关板端实验全部通过，不能据此声称全部边界或 CI 串口回归通过。本次不修改业务源码，不推送远端。

## 已补充的文档

- [RK3568 部署与测试](rk3568-deployment.md)：环境、sysroot、当前工具链、打包、GUI/headless、PTY 协议、异常输入、CTest 和证据模板。
- README 增加 ARM 部署和串口能力、使用入口和验证边界。
- 使用、协议、测试文档增加对应入口，避免读者只看到 TCP 功能。

## 优先补充：让别人能够复现

### 1. 版本号未同步

位置：`CMakeLists.txt` 的 `project(ADASim VERSION 2.8.1 ...)`。

最新提交说明是 V2.11，但编译生成的程序版本仍为 2.8.1。建议下一次源码修改统一为计划使用的版本号（例如 2.11.0），同步 README/tag。当前通过源码 SHA 和二进制 SHA256 识别产物，不仅凭 `--version` 判断。

### 2. 工具链是本机路径，且已更换编译器

位置：`cmake/rk3568-qt-toolchain.cmake`。

当前使用 `/usr/bin/aarch64-linux-gnu-g++` 和 `/home/topeet/...` sysroot，不能直接照搬早期 `aarch64-none-linux-gnu-g++` 的教程。新文档已按仓库现状说明。后续可将编译器/sysroot 改成可传入的 CACHE 参数，并在配置时检查存在性；不需要重新造工具链。

### 3. 缺少独立串口示例配置

位置：`config/adasim.ini`、`HeadlessRunner::setupSerial()`。

默认没有 `[serial]`，代码默认关闭串口。这是合理默认值，但读者不会自动知道如何启用。新文档提供单独的 `adasim-serial.ini` 操作步骤；后续可提交 `config/adasim-serial.example.ini`，明确修改设备路径，避免让默认 CI 配置依赖本地 PTY。

### 4. 安装包不包含场景

位置：`CMakeLists.txt` 的 install 规则。

当前只安装程序和默认配置，部署后直接执行场景命令会缺文件。新文档显式复制 `scenarios/`；后续可增加安装规则，让打包一步完成。安装命令会覆盖默认配置，板端自定义配置应独立保存。

## 优先补充：把串口实验变成可重复验证

### 5. CTest 没有串口覆盖

位置：`tests/CMakeLists.txt`、`.github/workflows/build-and-test.yml`。

当前注册五个条目，覆盖评估、场景、TCP、Planner 与 empty smoke，没有 SerialProtocol/SerialPort。CI 成功不等于新串口逻辑受回归保护。

适合当前实习项目范围的最小补充：

1. 一个协议单元测试：PING、GET_STATE、合法边界 0/30、缺参数、非法数字、负数/超范围、多余参数、空命令、非有限输入被拒绝。
2. 一个 Linux PTY 集成测试：分片、多行、超长行恢复、SET_SPEED/GET_STATE、退出与重新启动；设置总超时。
3. 先在宿主 Ubuntu CI 跑这些测试。暂时不要求搭建 ARM 云端运行环境。

现有手动清单已写入部署文档；本次没有新增测试实现或伪造测试结果。

### 6. 自动场景测试可被串口输入改变

位置：`HeadlessRunner::setupSerial()` / `handleSerialLine()`。

代码没有在 `--test` 时禁止串口修改目标速度。若自动场景配置启用了串口且对端发送控制命令，测试结果可能受外部输入影响。

目前文档要求自动场景测试用串口关闭的独立配置。后续可在测试模式拒绝启用串口，或明确将外部输入作为受控测试负载；不用立即扩展复杂的测试框架。

## 后续可选，暂不阻塞展示

- Qt Test 当前无条件查找，即使 `BUILD_TESTING=OFF` 也需要开发配置。可把测试依赖查找放到条件中；运行时 TestEvaluator 不依赖 Qt Test，二者不要混淆。
- 交叉构建的宿主 Qt 工具覆盖仅设置在 ADASim 目标；当前文档使用 `BUILD_TESTING=OFF`。若将来交叉构建测试目标，需要对这些目标一并配置宿主工具，并在 ARM 环境执行测试。
- 串口波特率固定 115200、8N1、无流控；当前 INI 没有 baud 参数，不应写成支持任意波特率配置。
- 串口初始化失败会终止初始化；运行中 I/O 失败仅关闭串口并继续仿真；没有自动重连或对端超时策略。先展示明确的现有行为即可。
- 真实 UART 不会因为 PTY 成功就自动验证通过；有硬件后再检查电平、引脚复用、接线和串口参数。
- README 已披露 Python Planner 和横向控制闭环的历史限制，本次没有修改或证明这些问题已解决。

## 下一步建议

先把本次文档与运行证据提交；随后补一个串口协议测试。完成后再进入 systemd 部署，暂不同时扩展真实硬件、复杂重连和新算法。
