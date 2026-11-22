# MuShiBot ESP32 固件源码

这是独立、可阅读的固件源码副本，保留当前工作树控制逻辑及 u2594 Actor；不含真实网络凭据、训练 checkpoint、编译器、二进制或旧 Git 历史。中文注释说明状态所有权和控制边界，第三方库及生成模型参数保持原字节。

**状态**：当前模型元数据仍有 `hardware_deployment_prohibited: true`。编译/离线回归不等于实机稳定性、安全性或模型部署验收；不要用编译测试产物直接烧录。启动 `initFOC()` 会驱动轮对齐，“上电停止”不等于初始化期间完全无动作。

## 目录

- `src/app`：启动、主循环、模式切换与唯一执行顺序。
- `src/control/native`：零摇杆原生站立；`ai`：Actor适配、历史及动作推进。
- `src/hardware`：引脚、IMU、电池、轮电机与舵机唯一所有者。
- `src/network`：网页、NVS网络配置、HTTP和TCP；`telemetry`：固定容量跨核桥。
- `model`：生成参数和身份合同，参数不要手工排版或修改。
- `protocol`、`tools`：帧定义、离线解析及构建；`tests`：不接硬件的回归。
- `lib`、`licenses`：带本地补丁的供应源码和许可；见 THIRD_PARTY.md。

## 快速开始：仅编译

安装 Python 3 和 PlatformIO Core（固定版本在 requirements-build.txt）：

```sh
python -m pip install -r requirements-build.txt
python tools/check_source.py
python tools/build.py --build-only
```

所有命令在项目根执行；脚本按自身位置定位，外部目录调用时写完整脚本路径。可通过 `--pio "PlatformIO可执行文件路径"` 指定已有工具，不需要修改全局PATH。平台、Arduino包和Xtensa工具链已固定在 platformio.ini；首次构建需下载这些工具。

`--build-only` 在生成目录创建**合成网络配置**，没有读取或复制用户密码；只验证编译链接。它拒绝 upload/uploadfs/program，不额外限制正常本地网络配置。日志默认 `build/build.log`，输出在 `.pio/build/esp32dev/`；这些都不是公开源码，也不应提交。可用 `--log 外部日志路径` 改日志位置。

## 普通本地构建与配置

1. 复制 `include/secrets.example.h` 为同目录 `secrets.h`。
2. 仅在本机编辑四个宏，删除模板 `#error`。空STA表示不启用默认STA；AP开放或加密配置应按ESP32 Wi-Fi规则和自身风险选择，源码不附加密码静态断言。
3. 执行 `python tools/build.py`。缺少本地文件会明确失败，不自动使用测试配置。
4. 不提交 secrets.h、构建产物或设备NVS。普通构建产物可能嵌入凭据。

完整硬件、控制、网络和时序参数见 [配置参考](docs/CONFIGURATION.md)。项目没有预置真实设备SSID、IP或用户名。已有设备 `wifi_cfg` NVS可能覆盖STA默认宏；本副本不读取、不迁移设备NVS。

## 手动烧录/串口说明（本次未执行）

本仓保留候选模型的禁止硬件部署标记，不能仅改JSON将其“批准”。完成模型、接线、电压、方向及机械保护的独立实机审查后，操作者可在**自行配置的普通构建环境**手动运行标准PlatformIO命令：

```sh
pio run -e esp32dev -t upload --upload-port <本机端口>
pio device monitor --port <本机端口> --baud 115200
```

不要对合成配置运行上传；本次只构建与离线测试，没有执行上述命令、连接串口或操作机器人。

## 网络连接与操作

使用本地配置的热点或STA网络连接，HTTP端口80。在串口启动诊断中确认分配地址，然后浏览器访问该地址（不要猜固定IP）。AP+STA同时启用；保存配置优先、失败重连的行为见配置文档。

上电应用模式为停止，舵机动力意图关闭；初始化轮FOC仍可能动作。网页先“取得控制会话”，再手动选择原生站立；AI只在原生至少500ms且接管条件通过时接纳。状态可读不代表控制权限；命令202只代表入队，必须查看新鲜的实际模式。

按钮含义：原生站立=冷入原生；AI=尝试接管；回原生=保留原生状态回切；停止/收腿=停轮且保持腿动力意图；舵机失能=停轮并解除腿动力。**关闭网页不保证停止**：1500ms心跳失联会让运行中的AI交回原生；已在原生则仍可保持原生。硬故障与人工停止不会因失联而自动解除。操作时使用独立机械保护。

## 日志和离线测试

TCP 9871只发送日志，支持单客户端；首行JSON身份，后接516字节帧。固定8帧队列满时丢帧计数增长，不等待网络；不承诺无损。采集端须自行完整保存TCP字节流，不要把HTTP状态当全量日志。离线解析：

```sh
python tools/parse.py capture.raw --output frames.jsonl
```

输出必须不存在，避免覆盖；摘要含身份、尾部半帧、序列缺口和原生/AI分组统计。遥测字段区分命令、估计与实测。

主机需 C++11 编译器 `c++` 和 Node.js，Python仅用标准库：

```sh
python tests/run_host.py
python tests/review_main.py
python tests/test_direct_timing.py
python tests/test_direct_contract.py
python tests/test_protocol.py
python tests/test_telemetry_header.py
node tests/review_page.js
```

覆盖真实C++控制/Actor前向、C++帧跨语言解析、主循环硬件桩、推理边界与网页沙箱。不访问硬件。Windows固件编译与macOS离线回归分别验收，不能混称Windows已运行主机C++测试。

## 排错

- 缺 secrets.h：本地配置后普通构建，或仅编译用 --build-only；不要把密码发到聊天。
- 供应库/Actor指纹失败：恢复锁定字节，核对 dependencies.lock.json 和模型合同，不删除校验。
- 网页403/409：控制会话过期或其他会话活跃；重新取得权限，不自动争抢。
- 状态过期/模式未知：检查网络和独立心跳提示，不能假定机器人已停。
- 不能进AI：先查实际原生持续时间、姿态和reason；未标定模型不能凭按钮成功认定可用。
- 启动失败：串口 `[BOOT]` 分阶段输出桥、I2C/IMU/FOC/网络；`/diagnostics` 只保留32个事件及首个硬故障，不是无限历史。
- TCP断开或缺帧：慢客户端1500ms无进展会断开；检查drops和序列，原始日志可能有尾部半帧。

## 模型替换与来源

Actor为63→64→64→4 tanh生成网络。不能单独更换参数、观测顺序或合同哈希；需配套导出身份、动作单位、轴向、历史、时序及离线前向一致性验证，另行实机验收。本源码不含checkpoint，身份校验不冒称验证训练冻结来源。见 [架构](docs/ARCHITECTURE.md)、[第三方声明](THIRD_PARTY.md)。各组件沿用原许可，不给全工程统一套MIT，也不新增重新申请授权门槛。
