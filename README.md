[English](README.en.md) | [简体中文](README.md)

# MuShiBot ESP32 固件

微型轮腿机器人的固件源码：原生站立控制、设备端神经网络 Actor、浏览器控制页与遥测。本 README 是供人和代码代理共同阅读的工程入口：先看事实与阅读顺序，再沿源码路径核对后修改行为。

**随附候选模型禁止硬件部署：**[model/active-model.json](model/active-model.json) 中保留 `hardware_deployment_prohibited: true`。编译、离线测试和身份校验不代表实机安全或稳定性批准。不得烧录仅编译产物。即便应用启动为停止态，启动时的 `initFOC()` 仍可能驱动轮对齐。

## 1. 快速事实与范围

| 项目 | 契约 |
|---|---|
| 目标 | ESP32、Arduino、PlatformIO 环境 `esp32dev`；该板型设置不能证明实物模组或 Flash 容量 |
| 构建版本 | PlatformIO Core 6.1.18；espressif32 6.10.0；Arduino 包 3.20017.241212+sha.dcc1105b；Xtensa 8.4.0+2021r2-patch5 |
| 原生控制器 | 保留 PID/滤波状态的零摇杆站立；不是运行时摇杆、跳跃或任意高度接口 |
| AI 控制器 | 本地 u2594 Actor，`63 → 64 → 64 → 4`，tanh；不含云服务、LLM 或固件 Critic |
| 输入输出 | 21 个值 × 3 帧历史；输出依次为左腿位置、右腿位置、左轮电压、右轮电压 |
| 网络 | HTTP 80 提供页面/状态/控制；TCP 9871 提供单客户端遥测；仅限受信局域网 |
| 包含 | 固件、生成 Actor/合同、带补丁供应库、构建工具、离线测试及组件许可 |
| 不包含 | 真实网络凭据、设备 NVS、训练 checkpoint、编译器、二进制、缓存和旧 Git 历史 |

## 2. 推荐阅读顺序

1. 本 README：范围、安全边界、构建模式与运行语义。
2. [SETUP.md](SETUP.md) 与 [docs/CONFIGURATION.md](docs/CONFIGURATION.md)：初始化背景、引脚、单位、限位、网络配置与时序。
3. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)，然后 [src/app/main.cpp](src/app/main.cpp)：状态归属和实际 step 1–9 执行顺序。
4. 按下方工程地图阅读待修改模块，先追调用方及最终硬件提交路径。
5. 修改 AI 适配或动作语义前，读 [model/active-model.json](model/active-model.json) 和 [deployment/direct_contract.py](deployment/direct_contract.py)。
6. 修改依赖前，读 [THIRD_PARTY.md](THIRD_PARTY.md)、[dependencies.lock.json](dependencies.lock.json) 和 [lib/PATCHES.md](lib/PATCHES.md)。[docs/INDEX.md](docs/INDEX.md) 索引现有文档，这些文档目前使用中文。

## 3. 工程地图与数据流

| 路径 | 职责与边界 |
|---|---|
| [src/app/main.cpp](src/app/main.cpp) | 启动、模式状态与唯一控制主循环顺序；控制核拥有控制器状态 |
| [src/control/native/](src/control/native/) | 原生站立 PID/滤波状态；回切保留状态并重锚时钟/距离，不承诺无扰交权 |
| [src/control/ai/](src/control/ai/) | 坐标适配、三帧历史、推理及动作推进；`FourCore.h` 不访问硬件 |
| [src/control/Safety.h](src/control/Safety.h) | 公共校验、硬故障条件和输出限幅 |
| [src/hardware/](src/hardware/) | IMU、电池、轮电机和舵机的唯一所有者；`sample()` 采集，`apply()` 提交 |
| [src/telemetry/Bridge.h](src/telemetry/Bridge.h) | 固定容量跨核快照、命令、状态与事件；控制不等待网络 |
| [src/network/Network.cpp](src/network/Network.cpp) / [Page.h](src/network/Page.h) | 核 0 拥有 Wi-Fi/NVS/HTTP/TCP；网页心跳与状态轮询独立运行 |
| [protocol/fields.def](protocol/fields.def) / [Frame.h](protocol/Frame.h) | C++/Python 共享线格式：每帧 516 字节 |
| [model/](model/) / [deployment/](deployment/) | 生成参数、模型身份和部署合同校验；checkpoint 不是编译依赖 |
| [tools/](tools/) / [tests/](tests/) | 构建、身份校验、离线解码及无硬件回归 |
| [lib/](lib/) / [licenses/](licenses/) | 带本地补丁的供应库和保留许可；不得替换为无补丁上游版本 |

控制流：硬件采样 → 有限输入/停止检查 → 原生或 AI 控制器 → 候选输出保护与限幅 → 唯一 `hardware::apply()` 提交。保留 `main.cpp` 的准确顺序，其他模块不得直接写电机。AI 内部的观测 → 旧动作推进 → 推理完成 → 新动作接纳有先后契约，不得重排。

网络流：浏览器命令 → 有界桥 → 控制核；冻结控制快照 → 有界桥 → HTTP 状态/TCP 日志。网络只消费副本，不执行控制计算，不修改冻结帧。

## 4. 快速开始：仅编译

通过标准 HTTPS 地址克隆，然后在工程根执行：

```sh
git clone https://github.com/dycsrl/Micro-Wheeled_leg-Robot-AI-Firmware.git
cd Micro-Wheeled_leg-Robot-AI-Firmware
python -m pip install -r requirements-build.txt
python tools/build.py --build-only
```

需要 Python 3 与 PlatformIO Core；Core 固定版本见 [requirements-build.txt](requirements-build.txt)。首次构建会下载锁定的平台、框架和工具链。已有安装可用 `python tools/build.py --pio "<PlatformIO executable>" --build-only` 指定。包装脚本按自身位置定位工程，也可在其他目录使用脚本绝对路径调用。

`--build-only` 生成**合成网络配置**，不读取或复制本地密码。构建钩子在该模式拒绝 `upload`、`uploadfs`、`program`；包装脚本本身只构建。默认日志为 `build/build.log`，产物位于 `.pio/build/esp32dev/`；`--log <path>` 可改变日志位置。生成产物不是源码，不得提交或烧录。

**源码检查限制：**`python tools/check_source.py` 校验 Actor 合同、供应库哈希和 Python 语法，但当前还断言工程根没有 `.git`。仅在不含 Git 元数据的导出源码目录运行，例如解压 GitHub 源码归档后执行。普通克隆即便通过前述检查也会在这里失败；不要删除 `.git` 或绕过断言掩盖限制。本次仅修改 README，不包含代码修复。

## 5. 普通本地构建与凭据

1. 在本机将 [include/secrets.example.h](include/secrets.example.h) 复制为 `include/secrets.h`。
2. 填写 `STA_SSID`、`STA_PASS`、`AP_SSID`、`AP_PSW`，再删除模板 `#error`。默认 STA SSID 为空时不启动默认 STA。开放或加密 AP 应按 ESP32 Wi-Fi 规则与自身风险选择；源码不附加密码长度断言。
3. 执行 `python tools/build.py`。缺少本地凭据文件会明确失败，普通构建不会静默退到合成配置。
4. 不将 `include/secrets.h`、设备 NVS、日志和构建产物纳入 Git。普通二进制可能嵌入凭据，不要把密码发到聊天。

已有 `wifi_cfg` NVS 可能覆盖编译时 STA 默认值。本源码副本初始化时不读取或迁移设备 NVS。当前网页没有保存网络设置的接口，不要虚构。优先级与静态地址回退规则见[配置参考](docs/CONFIGURATION.md)。

手动烧录**不是**快速开始步骤。只有完成模型、接线、供电电压、方向及机械保护的独立审查后，操作者才可在自行配置的普通构建环境使用：

```sh
pio run -e esp32dev -t upload --upload-port <port>
pio device monitor --port <port> --baud 115200
```

不得上传合成配置。修改 JSON 禁止标记不等于获得部署批准。本次文档更新不执行烧录、串口连接或机器人操作。

## 6. 运行模式与安全边界

通过本机配置的 AP 或 STA 网络连接；工作模式为 AP+STA。以串口启动诊断给出的分配地址为准，不假定固定 IP，然后通过 HTTP 80 访问。项目没有 HTTPS、用户管理或公网加固；控制租约不是 TLS 认证。不得直接暴露公网。

- 应用启动为停止态，舵机动力意图关闭；FOC 初始化仍可能驱动轮子。
- 发模式命令前先取得控制会话。状态可读不代表拥有控制权。HTTP 202 仅代表入队而非完成，须查看新鲜的实际模式/状态。
- 原生站立为冷进入。AI 接管要求原生至少 500ms、`abs(pitch-nativePitchZero) < 20deg`、输入有效且种子成功；否则报告 `NotStanding`。
- 回原生保留原生状态并重锚。停止/收腿停轮但保留腿动力意图；舵机失能停轮并解除腿动力。
- **关闭网页不保证停止。**1500ms 心跳租约失效时，运行中的 AI 在硬检查通过后交回原生；已处于原生则可继续原生控制。失联不会自动解除硬故障和人工停止。
- 硬检查包括有限值、`accZ > 1.8g`、有效电池、`abs(pitch) > 25deg` 或 `abs(roll) > 35deg`、`elapsed > 100000us`。硬故障停机；控制期限/Actor 失败或心跳失联，仅在运行态且硬检查通过时回原生。

HTTP 路由：`GET /`、`/status`、`/diagnostics`；`POST /claim`、`/heartbeat`、`/command`（含 token 和 code）。命令码：`0` 停止、`1` 原生、`2` AI、`3` 回原生、`4` 舵机失能。命令超过 500ms 失效，停止锁优先。HTTP 403 表示未授权，409 表示存在活动所有者/冲突。应由操作者主动重新取得权限，不自动争抢。操作时使用独立机械保护。

## 7. AI 合同与修改约束

- u2594 使用 63 维 Actor 输入（21 × 3 帧），不是训练侧 65 维 Critic 观测。输入顺序/尺度见[模型合同](model/active-model.json) 的 `actor_frame_names` / `actor_scales`；`critic_privileged` 为空，不据此虚构额外固件通道。
- 每个动作裁剪到 ±1。输出顺序为左腿、右腿、左轮、右轮。腿期望角为 `clip(-.3297 + .12*a, -.5, -.12)` **rad**；轮命令为 `7.4*a` **V**，保持到下次接纳动作，无轮 LPF/P 环。硬件输出保护仍生效。
- 腿每 1ms 步的 target 最大变化 `.003rad`；estimate 按 `.025*(desired-estimate)` 推进。这是目标历史估计，**不是实测舵机反馈**。适配器将角/角速度换算为 rad/rad/s，双轮角、速、电压同时反号；不扣动态原生 pitch 零点，不交换轮通道。
- 合同控制步为 **10ms**，仿真步为 **1ms**，训练动作延迟为 **7ms**。这些不是固件 sleep。固件同步推理，不得人为加入 7ms 延迟。推进使用实际微秒差，超过 100ms 失败；AI 循环不承诺强制 10ms 墙钟周期。非 AI 的 `vTaskDelay(1)` 加至少 1500us 等待也不是固定 1ms 控制环。
- `model/Actor.h` 必须逐字节匹配 `actor_sha256`。不得手动排版生成参数、单独替换权重、重排观测、改变单位或移除身份/保护检查。替换模型需配套导出身份、合同、轴向、历史、时序和离线前向一致性验证，再独立进行硬件审查。
- 元数据中的 checkpoint 路径是来源标识，不是随附运行时依赖。身份校验不能验证完整训练冻结来源。真实轴向、零偏与带载舵机行为尚未标定；保留 `hardware_deployment_prohibited: true`。

## 8. 遥测与离线回归

TCP 9871 只发送日志：一行 JSON 身份，之后为 516 字节帧；单客户端，无认证或加密。接收端自行完整保存原始字节流。8 帧队列满时丢弃，不等待网络；drops 与序列缺口说明日志并非无损。客户端 1500ms 无发送进展则断开。HTTP 状态不是全量日志。

```sh
python tools/parse.py capture.raw --output frames.jsonl
```

输出路径必须尚不存在。摘要包含身份、尾部半帧、序列缺口及原生/AI 分组统计。须区分命令电压、估计腿角和实测传感器，不能混用。

主机测试需要 Python 3（标准库）、可通过 `c++` 调用的 C++11 编译器，以及 Node.js。脚本直接调用 `c++`；仅安装 Windows 固件工具链不能满足该主机测试依赖。

```sh
python tests/run_host.py
python tests/review_main.py
python tests/test_direct_timing.py
python tests/test_direct_contract.py
python tests/test_protocol.py
python tests/test_telemetry_header.py
node tests/review_page.js
```

覆盖 C++ 控制/Actor 前向、C++/Python 线格式解码、硬件桩主循环、推理边界及网页沙箱，不访问硬件。

## 9. 验证状态与排错

此前工程验证区分 **Windows 固件编译**与 **macOS 离线回归**，不声称 Windows 主机 C++ 测试或硬件部署通过。本次仅更新 README，不重跑固件编译，不修改代码、模型字节、保护逻辑或依赖。实机稳定性/安全性、带载标定、烧录及机器人端到端运行在此仍未验收。

| 症状 | 优先检查 |
|---|---|
| 缺少 secrets | 普通构建先在本机配置；仅编译才用 `--build-only` |
| 源码检查因 `.git` 失败 | 按第 4 节使用导出源码目录，保留仓库元数据 |
| 供应库/Actor 指纹不符 | 恢复锁定字节，查 `dependencies.lock.json` 与模型合同，不删除检查 |
| 状态过期/模式未知 | 查网络和独立心跳，不能假定机器人已停止 |
| AI 进入被拒绝 | 查实际原生持续时间、姿态和 reason；按钮被接纳不等于模型验证 |
| 启动失败 | 查分阶段 `[BOOT]` 诊断；`/diagnostics` 仅保留 32 个事件和首个硬故障，不是无限历史 |
| TCP 缺帧/断开 | 查 drops、序列号、慢客户端超时与尾部半帧 |

## 10. 来源与许可

- 原机器人硬件/控制：[MuShibo/Micro-Wheeled_leg-Robot](https://github.com/MuShibo/Micro-Wheeled_leg-Robot)，贡献者 **穆世博（Mu Shibo / MuShibo）**与 **李育锋（Li Yufeng）**。此前来源核对未发现独立 LICENSE；公开可读不代表全工程为 MIT。保留原声明与使用条件。
- 部署接口参考：[星际浪子 / mujoco_esp32_robot_deploy_public](https://gitee.com/starimpact_admin/mujoco_esp32_robot_deploy_public)，MIT，Copyright (c) 2024 星际浪子；原文保存在 [licenses/](licenses/)。
- 供应 Simple FOC 2.2.1、MPU6050_tockn 1.5.2、SCServo/SCSservo 1.0.1 保留原源码声明、补丁及适用许可。SCServo 附 GPL-3.0 参考许可，不改称 MIT。

准确来源、组件许可事实与再分发义务见 [THIRD_PARTY.md](THIRD_PARTY.md)，本地补丁见 [lib/PATCHES.md](lib/PATCHES.md)。**不将全工程重新授予 MIT**，也不新增全工程重新申请授权门槛。未包含的训练/机械资产不由本 README 授权。
