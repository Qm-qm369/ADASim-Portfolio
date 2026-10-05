# RK3568 交叉编译、部署与串口测试

本文按提交 `5f2fb1965c3965b2ae08b11ddbe3980cea7892e7`（提交说明为 V2.11）的源码编写。命令面向 Ubuntu 虚拟机和迅为 RK3568，不能在 Windows PowerShell 中原样运行。

作者已在学习过程中反馈交叉部署、PTY 串口控制和状态查询通过；本文中的输出是验收预期，不代表文档检查者重新执行了板端测试。真实 UART 电气通信、长期稳定性和串口自动化回归尚未由此验证。

## 1. 环境与当前边界

| 环境 | 用途 / 已知信息 |
|---|---|
| Ubuntu 虚拟机 | x86-64；运行交叉编译器和宿主机 Qt 工具；实际发行版请用 `/etc/os-release` 确认 |
| RK3568 | AArch64，Ubuntu 20.04.6，Qt 5.12.8 |
| 宿主机 Qt 工具 | `/usr/lib/qt5/bin/{moc,uic,rcc}`，已反馈版本 5.12.8、x86-64 |
| sysroot | 板端头文件、ARM 库、Qt CMake 配置的本地副本 |
| 当前仓库编译器 | `/usr/bin/aarch64-linux-gnu-g++`；与早期练习的 `aarch64-none-linux-gnu-g++` 不同 |

当前构建仍链接 Qt Core / Gui / Widgets / Network，且无条件查找 Qt Test。`--headless` 是运行模式，不会消除 GUI 构建依赖；`BUILD_TESTING=OFF` 只关闭独立测试目标。串口目前仅装配在 `HeadlessRunner`，GUI 不处理串口命令。

## 2. 检查环境

### 虚拟机

```bash
cat /etc/os-release
uname -m
cmake --version
/usr/bin/aarch64-linux-gnu-g++ --version
/usr/bin/aarch64-linux-gnu-g++ -dumpmachine
/usr/lib/qt5/bin/moc -v
/usr/lib/qt5/bin/uic -v
/usr/lib/qt5/bin/rcc -v
file -L /usr/lib/qt5/bin/moc
```

缺少交叉编译器时，Ubuntu 可安装 `g++-aarch64-linux-gnu`；还需 CMake、Make、rsync 和匹配的宿主机 Qt 工具。不要在未核对版本时把不同发行版的 Qt 工具混用。当前板端 Qt 为 5.12.8，优先复用已经验证成功的同版本工具。

### 开发板

```bash
uname -m
cat /etc/os-release
qmake -v
dpkg-query -W -f='${Package} ${Architecture} ${Version}\n' \
  qtbase5-dev qtbase5-dev-tools qt5-qmake libc6-dev libstdc++6
ldd /usr/lib/aarch64-linux-gnu/libQt5Core.so.5
```

准备 sysroot 的板端需要开发文件，仅安装运行库不够。板端缺少开发包时可安装 `qtbase5-dev qtbase5-dev-tools qt5-qmake libc6-dev`。只运行部署包则不要求安装编译器。

## 3. 准备 sysroot（已有验证成功的副本可跳过）

在虚拟机执行，替换板端 IP；双方需要 `rsync`，板端已能通过 SSH 登录。

```bash
RK_BOARD=topeet@192.168.1.100
RK_SYSROOT="$HOME/embedded-learning/sysroots/rk3568-ubuntu20"
mkdir -p "$RK_SYSROOT/lib" "$RK_SYSROOT/usr/include" "$RK_SYSROOT/usr/lib"

rsync -a --info=progress2 "$RK_BOARD:/usr/include/" "$RK_SYSROOT/usr/include/"
rsync -a --info=progress2 "$RK_BOARD:/usr/lib/" "$RK_SYSROOT/usr/lib/"
rsync -a --info=progress2 "$RK_BOARD:/lib/" "$RK_SYSROOT/lib/"
```

每条命令应成功完成；若有权限或空间错误，先修复，不继续使用不完整副本。此处保留完整目录，优先减少 Qt 的间接依赖遗漏。

将副本中的绝对符号链接转换为指向副本内部的相对链接，避免落到宿主机系统目录。下列脚本仅适用于这里约定的 sysroot 路径：

```bash
export RK_SYSROOT
python3 - <<'PY'
import os
from pathlib import Path

root = Path(os.environ['RK_SYSROOT']).resolve()
expected = (Path.home() / 'embedded-learning/sysroots/rk3568-ubuntu20').resolve()
if root != expected or root == Path('/'):
    raise SystemExit(f'Unexpected sysroot: {root}')
changed = 0
for directory, dirs, files in os.walk(root, followlinks=False):
    for name in dirs + files:
        link = Path(directory) / name
        if not link.is_symlink():
            continue
        target = os.readlink(link)
        if not os.path.isabs(target):
            continue
        relative = os.path.relpath(root / target.lstrip('/'), link.parent)
        link.unlink()
        link.symlink_to(relative)
        changed += 1
print(f'Converted links: {changed}')
PY
```

重新同步板端文件后，需要重新检查这些链接。转换不会自动补齐缺失文件。

```bash
ls "$RK_SYSROOT/usr/include/aarch64-linux-gnu/qt5/QtCore/qglobal.h"
ls "$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5/Qt5Config.cmake"
file -L "$RK_SYSROOT/usr/lib/aarch64-linux-gnu/libQt5Core.so"
```

预期头文件和配置存在，动态库显示 `ARM aarch64`，不是损坏链接。

## 4. 在虚拟机交叉构建

首次获取源码：

```bash
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/Qm-qm369/ADASim-Portfolio.git
cd ADASim-Portfolio
git rev-parse HEAD
```

已有源码则直接进入其根目录。先保存本地修改，不要覆盖未提交的工作。

检查 `cmake/rk3568-qt-toolchain.cmake`：当前编译器和 sysroot 都写了绝对路径。用户名、安装位置不同时，需要修改对应的 `CMAKE_CXX_COMPILER`、`CMAKE_SYSROOT`。不要以为设置 shell 变量就会覆盖工具链文件中的硬编码路径。

若更换了编译器或 sysroot，使用新的构建目录，不复用旧缓存。以下假定目录 `build-rk-cross` 尚未用于其他工具链。

```bash
RK_SYSROOT="$HOME/embedded-learning/sysroots/rk3568-ubuntu20"

cmake -S . -B build-rk-cross \
  -DCMAKE_TOOLCHAIN_FILE=cmake/rk3568-qt-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF \
  -DRK_HOST_QT_BIN=/usr/lib/qt5/bin \
  -DQT_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5" \
  -DQt5_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5" \
  -DQt5Core_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5Core" \
  -DQt5Gui_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5Gui" \
  -DQt5Widgets_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5Widgets" \
  -DQt5Network_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5Network" \
  -DQt5Test_DIR="$RK_SYSROOT/usr/lib/aarch64-linux-gnu/cmake/Qt5Test"

cmake --build build-rk-cross --parallel 2
file build-rk-cross/ADASim
```

预期完成构建，产物架构为 AArch64。详细诊断可使用 `cmake --build build-rk-cross --verbose`。

不在 x86 虚拟机直接运行 ARM 产物，也不对它使用宿主机 `ldd`；依赖加载检查放在开发板。

## 5. 打包与复制

在虚拟机仓库根目录执行：

```bash
cmake --install build-rk-cross --prefix "$PWD/package-rk3568"
mkdir -p package-rk3568/scenarios
cp -a scenarios/. package-rk3568/scenarios/
git rev-parse HEAD > package-rk3568/source-commit.txt
sha256sum package-rk3568/bin/ADASim
```

有未提交的源码修改时，额外保存 `git diff`；SHA 只标识提交，不包含工作区修改。

```text
package-rk3568/
├── bin/ADASim
├── bin/config/adasim.ini
├── scenarios/
└── source-commit.txt
```

当前安装规则不复制场景，所以上面的 `cp` 是必要步骤。运行库暂时复用板端已安装的兼容 Qt，不随包覆盖系统库。

板端创建目录：

```bash
mkdir -p ~/ADASim/deploy
```

虚拟机复制（替换 IP）：

```bash
scp -r package-rk3568 topeet@192.168.1.100:/home/topeet/ADASim/deploy/
```

更新已有包前先停止程序、备份板端自定义配置；重新复制整个包会覆盖默认配置。使用普通用户 `topeet` 运行，避免输出文件变成 root 所有。

## 6. 板端基础验收

在开发板执行：

```bash
cd ~/ADASim/deploy/package-rk3568
ldd ./bin/ADASim
sha256sum ./bin/ADASim
./bin/ADASim --headless --help
```

依赖没有 `not found`；SHA256 与虚拟机对应文件一致。当前仓库程序版本仍由 CMake 的 `2.8.1` 生成，不要仅凭 `--version` 判断是否复制了最新 V2.11 代码。

```bash
./bin/ADASim --headless --config ./bin/config/adasim.ini
```

预期持续输出 `[SIM]`。约 30 秒后按 Ctrl+C，然后立即记录退出码：

```bash
result=$?
printf 'exit code: %s\n' "$result"
head -n 5 record/simulation.csv
```

预期退出为 0，CSV 有表头和数据。`record/`、`test_result/` 相对于启动工作目录；重复运行会覆盖相关输出，重要记录先备份。`TTC=-1` 表示没有有效 TTC 样本。

### GUI（可选）

在开发板图形桌面的终端中，以桌面登录用户执行：

```bash
cd ~/ADASim/deploy/package-rk3568
./bin/ADASim --config ./bin/config/adasim.ini
```

验证窗口显示、启动、暂停、继续、停止、关闭及退出码。不加 `--headless`；普通 SSH 会话不保证能访问显示会话。不要通过混入其他版本的 Qt 插件来修复显示错误。当前 GUI 不接入本节串口命令。

## 7. PTY 串口控制测试

所有操作均在开发板完成。停止其他 ADASim 和串口练习程序，避免端口 8080 冲突及多个进程争抢同一个 PTY。

### 终端 A：保持虚拟串口对运行

```bash
sudo apt update
sudo apt install socat
mkdir -p ~/ADASim/serial-lab
socat -d -d \
  pty,raw,echo=0,link="$HOME/ADASim/serial-lab/ttyADASim" \
  pty,raw,echo=0,link="$HOME/ADASim/serial-lab/ttyPeer"
```

预期输出两个 `/dev/pts/N`，保持进程运行。固定链接名用于配置，不能把 PTY 数字写死。

### 终端 B：创建独立串口配置并启动

```bash
cd ~/ADASim/deploy/package-rk3568
cp bin/config/adasim.ini bin/config/adasim-serial.ini
nano bin/config/adasim-serial.ini
```

添加以下段落；如果已存在则修改，不能重复添加多个 `[serial]`：

```ini
[serial]
enabled=true
device=/home/topeet/ADASim/serial-lab/ttyADASim
```

用户名不同时修改完整路径，不使用 `~`。标准配置保持串口关闭，用于独立场景测试。

```bash
./bin/ADASim --headless --config ./bin/config/adasim-serial.ini
```

预期出现 `[SERIAL] Opened:` 及正常仿真输出。本实验不加 `--test`，不指定障碍物场景，也不连接 Python Planner。

### 终端 C：发送与接收

```bash
printf 'PING\n' > ~/ADASim/serial-lab/ttyPeer
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

预期 `PONG`。`timeout` 到期返回 124 是读取工具的行为，不是 ADASim 错误。每次等该命令结束再继续，只保留一个对端读取者。

```bash
printf 'SET_SPEED 3.0\nGET_STATE\n' > ~/ADASim/serial-lab/ttyPeer
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

预期回复形如：

```text
OK SET_SPEED 3.000
STATE target_speed_mps=3.000 speed_mps=7.800 aeb=0
```

`speed_mps` 是最近仿真帧的实际速度，示例值不是固定预期。`OK` 表示目标已接收；经过控制器调整，无遮挡场景的实际速度应逐渐接近 3 m/s，即终端显示约 10.8 km/h。

等待实际车速稳定后再发 `GET_STATE`，同时观察 CSV。不要把刚收到命令时的瞬时速度当成最终响应。

### 非法输入与分帧

```bash
printf 'SET_SPEED -1\nGET_STATE\n' > ~/ADASim/serial-lab/ttyPeer
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

预期 `ERR speed-out-of-range`，目标仍为 3.000。

```bash
printf 'SET_SPEED\nSET_SPEED abc\nPING extra\nHELLO\n' > ~/ADASim/serial-lab/ttyPeer
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

依次预期 `ERR invalid-speed`、`ERR invalid-speed`、`ERR unexpected-argument`、`ERR unknown-command`。

```bash
printf 'SET_SP' > ~/ADASim/serial-lab/ttyPeer
```

此时不应执行命令，仿真仍推进。随后：

```bash
printf 'EED 5.0\nGET_STATE\n' > ~/ADASim/serial-lab/ttyPeer
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

预期设置为 5.000；实际速度逐步趋近 18 km/h。

超长行后恢复：

```bash
python3 - <<'PY'
from pathlib import Path
with (Path.home() / 'ADASim/serial-lab/ttyPeer').open('wb', buffering=0) as port:
    port.write(b'A' * 65 + b'\nPING\n')
PY
timeout 2 cat ~/ADASim/serial-lab/ttyPeer
```

预期 `ERR line-too-long`，随后 `PONG`，证明超长输入没有破坏下一条命令。

### 退出与故障（分别执行）

1. 正常退出：终端 B 按 Ctrl+C，记录退出码 0、CSV 内容；保持 socat 运行，再次启动 ADASim，应能重新 PING。
2. PTY 断开：在 ADASim 运行时停止终端 A 的 socat。预期串口错误日志、串口关闭，仿真继续。重启 socat 不会自动重连，需重启 ADASim。
3. 错误设备路径：复制串口配置，把 device 改为不存在的路径再启动。预期初始化失败、错误日志、非零退出码（当前 headless 为 2）。恢复配置后重新运行。

PTY 断开与真实 UART 拔线不是同一种故障；真实 UART 可能只表现为长时间收不到数据。当前没有心跳失联策略和自动重连。

## 8. 协议与验证边界

| 项目 | 当前行为 |
|---|---|
| 串口参数 | 代码固定 115200、8N1、无流控；没有 INI 波特率配置 |
| 消息边界 | LF；接受末尾 CRLF；命令区分大小写 |
| 行长度 | LF 前最多 64 字节；末尾 CR 也占该长度；超长后丢弃到 LF |
| 目标速度 | 有限数值，0～30 m/s；不写回配置，重启按 INI 恢复 |
| 状态 | 用户目标速度、最近帧实际速度、AEB；首帧前返回 `ERR state-not-ready` |
| 发送队列 | 上限 4096 字节；积压超限关闭串口 |
| 线程 | HeadlessRunner 与串口通知共享主事件循环，无额外串口线程 |
| 平台 | Linux；非 Linux 开启串口会初始化失败 |

PTY 验证覆盖接口读写、分帧、参数解析和业务接入，不证明真实波特率、电平、接线、噪声处理或硬件流控正确。板端部署成功也不等于所有场景或长期稳定性测试通过。

## 9. 现有自动化测试与板端 smoke

### 在 Ubuntu 本机运行 CTest

使用本机编译器和独立构建目录，不传交叉工具链：

```bash
cmake -S . -B build-host-test -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-host-test --parallel 2
(cd build-host-test && ctest --output-on-failure --timeout 120)
```

括号写法兼容 Ubuntu 20.04 常见的 CMake 3.16。当前五个 CTest 条目不覆盖 SerialProtocol/SerialPort；串口测试按第 7 节单独记录。不要把交叉编译生成的 ARM 测试直接交给 x86 宿主机 CTest 运行。

### 在板端运行空场景 smoke

关闭其他 ADASim；使用未开启串口的标准配置，不连接 Planner：

```bash
cd ~/ADASim/deploy/package-rk3568
timeout 60 ./bin/ADASim --headless --test \
  --config ./bin/config/adasim.ini \
  --scenario ./scenarios/empty.json
result=$?
printf 'exit code: %s\n' "$result"
cat test_result/report.txt
```

预期退出 0、100 帧、PASS:1、AEB:0、Collision:0。若超时 124 或初始化失败，不得把上一次残留报告算作本次通过；检查文件时间与完整日志。

`straight_obstacle.json` 当前要求 AEB。车辆若常规减速停车却未触发 AEB，可能返回评估失败；这与 ELF、Qt 依赖或部署失败不同，不承诺整套场景回归通过。详见 [测试说明](testing.md)。

## 10. 常见问题

| 现象 | 优先检查 |
|---|---|
| 找不到编译器 | 当前仓库使用 `/usr/bin/aarch64-linux-gnu-g++`，核对工具链文件 |
| Exec format error | 是否把 ARM 的 moc 当成宿主工具，或在 x86 上直接运行 ARM 程序 |
| Qt 库格式不兼容 | Qt 配置是否落入宿主机 amd64 库，清楚区分 sysroot 与宿主工具 |
| 链接时报间接库缺失 | sysroot 是否完整、符号链接是否修正、rpath-link 是否匹配 |
| GLIBC / GLIBCXX version not found | 记录编译器、目标库版本；修正构建依赖，不直接覆盖板端系统 libc |
| 串口没有回复 | socat 是否存活、配置是否启用、有没有 LF、是否有多个读取者 |
| 端口 8080 被占用 | 关闭旧 ADASim，或修改 `[network] planner_port` |
| GUI 启动失败 | 板端桌面会话、用户权限、图形插件及其依赖 |
| CSV 无法写入 | 当前目录、目录权限、之前是否用 root 创建了文件 |

## 11. 保存测试记录

建议单独保存日期目录，不把输出中的“预期”直接标为“已通过”。

| 检查项 | 实测结果 | 证据路径 / 说明 |
|---|---|---|
| 源码 SHA、编译器与 Qt 版本 | 待填写 | |
| ARM 架构、部署文件校验和一致 | 待填写 | |
| 板端 ldd 无缺失 | 待填写 | |
| Headless、退出码、CSV | 待填写 | |
| GUI 基本交互（可选） | 待填写 | |
| PING、设置速度、查询实际速度 | 待填写 | |
| 非法输入不改变目标值 | 待填写 | |
| 分片、多消息、超长行恢复 | 待填写 | |
| 退出重启、PTY 断开 | 待填写 | |
| 本机 CTest、板端空场景 | 待填写 | |
| 真实 UART | 未验证 | 有硬件后补测 |

三分钟演示可按“展示 ARM 产物 → 板端启动 → 串口设置速度 → 查询实际变化 → 拒绝非法输入 → 正常退出并查看 CSV”的顺序。
