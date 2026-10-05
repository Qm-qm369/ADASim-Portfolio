# ADASim：RK3568 上的 systemd 后台部署

本文将已部署到 RK3568 的 ADASim 作为 Linux 系统服务运行，使用普通用户 `topeet`，通过 `systemctl` 启停、`journalctl` 查看日志。无需修改仿真算法，也不需要程序自行 fork 或使用 nohup。

## 1. 文件与环境

仓库提供：

- [服务文件](../deploy/systemd/adasim.service)
- [服务配置示例](../config/adasim-service.ini)

已反馈验证的板端环境为 Ubuntu 20.04.6、AArch64、Qt 5.12.8。本文命令均在开发板执行。假设程序已交叉编译并部署到 `/home/topeet/ADASim/deploy/package-rk3568/bin/ADASim`。

用户名和安装路径不同时，修改服务文件中的 `User`、`WorkingDirectory`、`ExecStart`，并同步调整本文路径。服务文件不依赖交互式 shell 的环境变量。

目录约定：

```text
/home/topeet/ADASim/
├── ADASim-Portfolio/                  # 源码仓库，路径可不同
├── deploy/package-rk3568/bin/ADASim   # 已部署程序
├── service-config/adasim.ini          # 服务专用配置
└── service-data/                     # 工作目录
    ├── record/simulation.csv
    └── test_result/
```

程序、配置与运行数据分开，更新程序时保留服务配置。当前 CSV 在重新启动时会被覆盖，需要保留的记录应先备份。

## 2. 安装前检查

```bash
ps -p 1 -o comm=
systemctl --version
id topeet
sudo -u topeet /home/topeet/ADASim/deploy/package-rk3568/bin/ADASim --headless --help
ldd /home/topeet/ADASim/deploy/package-rk3568/bin/ADASim
```

预期 PID 1 是 systemd，用户存在，程序能够输出帮助，依赖没有 `not found`。先停止手动运行的 ADASim，避免端口 8080 冲突。

## 3. 安装配置与服务文件

进入包含本次文件的源码仓库根目录，例如：

```bash
cd /home/topeet/ADASim/ADASim-Portfolio
sudo -u topeet mkdir -p \
  /home/topeet/ADASim/service-config \
  /home/topeet/ADASim/service-data/record \
  /home/topeet/ADASim/service-data/test_result
```

首次安装复制配置；已有自定义配置先备份再决定是否覆盖：

```bash
sudo install -o topeet -g "$(id -gn topeet)" -m 0640 \
  config/adasim-service.ini \
  /home/topeet/ADASim/service-config/adasim.ini

sudo install -o root -g root -m 0644 \
  deploy/systemd/adasim.service \
  /etc/systemd/system/adasim.service

sudo -u topeet test -w /home/topeet/ADASim/service-data/record
echo $?
```

权限检查应返回 0。若目录以前由 root 创建，应确认路径后修正这两个服务配置/数据目录的归属，不必修改整个 `/home` 的权限。

默认配置关闭串口，以免开机启动依赖临时 socat 进程；不传 `--test`，服务会持续仿真。

## 4. 关键配置含义

| 配置 | 作用 |
|---|---|
| `Type=simple` | systemd 直接管理前台主进程 |
| `User=topeet` | 普通用户运行；sudo 仅用于管理系统服务 |
| `WorkingDirectory` | 固定相对输出路径，不依赖启动命令的终端目录 |
| `Restart=on-failure` / `RestartSec=3` | 异常退出后等待 3 秒尝试重启 |
| `StartLimitIntervalSec=60` / `StartLimitBurst=3` | 限制短时间内反复启动，避免错误配置无限重试 |
| `KillSignal=SIGTERM` / `TimeoutStopSec=15` | 请求正常退出，最多等待 15 秒后由 systemd 强制清理 |
| `StandardOutput/StandardError=journal` | 收集程序输出 |
| `UMask=0027` | 限制新文件默认权限，CSV 通常为 0640 |
| `NoNewPrivileges=true` | 限制进程通过 exec 获得额外权限 |

`After=network.target` 只表达启动顺序，不保证互联网或某个远端服务可用。当前程序作为 TCP 服务监听，不依赖外部 Planner 已连接。

## 5. 启动、日志与用户验证

```bash
sudo systemd-analyze verify /etc/systemd/system/adasim.service
sudo systemctl daemon-reload
sudo systemctl start adasim
sudo systemctl status adasim --no-pager -l
systemctl is-active adasim
sudo journalctl -u adasim -b -n 30 --no-pager
```

预期 `active (running)`，日志有仿真启动和持续的 `[SIM]`。Type=simple 的启动返回不等于业务就绪，应结合日志判断。

```bash
ADASIM_PID=$(systemctl show adasim -p MainPID --value)
ps -o user,pid,ppid,etime,cmd -p "$ADASIM_PID"
sudo readlink "/proc/$ADASIM_PID/cwd"
```

预期进程用户为 topeet，工作目录为 `/home/topeet/ADASim/service-data`。

实时日志：

```bash
sudo journalctl -u adasim -f
```

Ctrl+C 只退出日志查看，不停止服务。`ADASim[PID]` 与 `adasim[PID]` 可分别来自 syslog 和标准输出；PID 相同不代表启动了两个进程。

## 6. 正常停止与数据验收

运行约半分钟后：

```bash
sudo systemctl stop adasim
systemctl is-active adasim
systemctl show adasim -p Result -p ExecMainCode -p ExecMainStatus
sudo journalctl -u adasim -b -n 20 --no-pager
ls -lh /home/topeet/ADASim/service-data/record/simulation.csv
head -n 5 /home/topeet/ADASim/service-data/record/simulation.csv
tail -n 5 /home/topeet/ADASim/service-data/record/simulation.csv
```

预期 inactive、Result=success、ExecMainStatus=0，日志包含收到 SIGTERM（15）和引擎停止，CSV 有表头与数据、属于 topeet。正常关闭会刷写记录；不能据此承诺强制终止或断电不丢数据。

保存本轮记录后，再测试从其他目录启动：

```bash
sudo -u topeet cp \
  /home/topeet/ADASim/service-data/record/simulation.csv \
  /home/topeet/ADASim/service-data/record/first-service-run.csv
cd /tmp
sudo systemctl start adasim
ADASIM_PID=$(systemctl show adasim -p MainPID --value)
sudo readlink "/proc/$ADASIM_PID/cwd"
```

工作目录仍应保持不变。

## 7. 异常重启实验（可选）

本实验强制杀死程序，未刷写数据可能丢失，重启后 CSV 会重新创建。重要数据先备份。

```bash
systemctl show adasim -p MainPID -p NRestarts
sudo systemctl kill --kill-who=main --signal=SIGKILL adasim
```

等待约 5 秒后：

```bash
systemctl show adasim -p MainPID -p NRestarts
sudo systemctl status adasim --no-pager -l
sudo journalctl -u adasim --since "5 minutes ago" --no-pager
```

预期新 PID、NRestarts 增加、服务重新 active。重启恢复进程，不恢复车辆状态或历史仿真进度。

`systemctl stop` 是主动停止，不会触发此自动重启。如果触发启动频率限制，先解决端口、路径或权限问题，再执行：

```bash
sudo systemctl reset-failed adasim
sudo systemctl start adasim
```

## 8. 开机启动与关闭

```bash
sudo systemctl enable --now adasim
systemctl is-enabled adasim
systemctl is-active adasim
```

预期 enabled、active。enable 只证明已配置自启动；实际重启开发板后，再执行以下命令才能记录“开机启动实测通过”：

```bash
systemctl is-active adasim
sudo journalctl -u adasim -b -n 30 --no-pager
```

停止并取消开机启动：

```bash
sudo systemctl disable --now adasim
```

预期 inactive、disabled。修改 service 文件后需要 daemon-reload；只改 INI 则重启 ADASim 即可。

## 9. 串口联调与使用边界

基础服务通过后，可在服务专用 INI 中启用：

```ini
[serial]
enabled=true
device=/home/topeet/ADASim/serial-lab/ttyADASim
```

先以 topeet 运行已验证的 socat PTY 对，再 `sudo systemctl restart adasim`。此时通过 PTY 的 PING、SET_SPEED、GET_STATE 验证后台控制。使用完整设备路径，不能写 `~`。

临时 PTY 未建立时服务会初始化失败；运行中 PTY 断开则串口关闭、仿真继续。当前程序不会自动重连，systemd 也不会因为串口关闭但进程仍存活而重启它。无人值守基础服务保持串口关闭；真实设备与设备权限配置以后再验证。

当前 CSV 没有自动轮转，重复启动会覆盖，长时间运行会持续增长。需要长期运行时先测量资源与存储变化，不把短时成功写成长稳通过。

## 10. 已反馈的验收记录

依据作者提供的 2026-10-05 板端终端输出整理，不是文档生成环境重新运行的测试。

| 项目 | 已观察到的结果 |
|---|---|
| 服务启动 | active (running)，持续输出仿真日志 |
| 普通用户 | ps 显示 topeet，主进程由 PID 1 管理 |
| 工作目录 | `/home/topeet/ADASim/service-data`，在 /tmp 启动也一致 |
| 正常退出 | SIGTERM 15，Result=success，ExecMainStatus=0 |
| CSV | 约 16K，所属 topeet:topeet，有表头及首尾记录 |
| 异常重启 | 主 PID 从 6302 变为 6424，NRestarts 从 0 变为 1 |
| 开机启动配置 | enable 后显示 enabled；随后执行过 disable --now |
| 实际重启自启动 | 所附记录没有展示，需单独补测 |
| 长时间稳定性 | 未由本轮短时测试验证 |

验证命令输出还包含其他服务的警告：snapd 的 RestartMode 不被识别、screan-sleep 文件有执行权限、rkaiq_3A 文件允许所有用户写入。这些不指向 adasim.service，未阻止本次服务验收；属于系统镜像维护事项，其中权限过宽应后续单独核查。

## 11. 常用命令

```bash
sudo systemctl start adasim
sudo systemctl stop adasim
sudo systemctl restart adasim
sudo systemctl status adasim --no-pager -l
sudo journalctl -u adasim -f
sudo journalctl -u adasim --since "10 minutes ago" --no-pager
```

本机权威说明可查 `man systemd.service`、`man systemd.exec`、`man journalctl`。
