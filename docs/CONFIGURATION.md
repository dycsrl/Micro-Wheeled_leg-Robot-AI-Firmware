# 配置参考

除“运行时”标注外，以下参数都是源码/构建配置，修改后需要重新编译；本次不改变原值。不要在运行中的控制环动态改PID或模型合同。数值来自本副本代码，不是硬件实测标定结果。

## 构建、硬件和坐标

| 配置 | 当前值/单位 | 位置与影响 |
|---|---|---|
| 环境 | esp32dev / espressif32 6.10.0 / Arduino | platformio.ini；板型配置不证明实际模组/Flash身份 |
| Arduino工具包 | 3.20017.241212+sha.dcc1105b | platformio.ini，保留精确版本 |
| Xtensa工具链 | 8.4.0+2021r2-patch5 | platformio.ini |
| 编译宏 | WIRE_SCL=15、WIRE_SDA=4，根目录include | platformio.ini；实际硬件总线在begin显式设置，不能仅凭宏接线 |
| 调试串口 | 115200 baud，TX缓冲1024字节 | main.cpp/setup；仅低频启动诊断，无串口控制协议 |
| 舵机串口 | Serial2，1000000 baud，ID 1/2 | Hardware.cpp/begin；未显式指定RX/TX，沿用板端Serial2默认，不推测接线 |
| 电机 | 两台7极对；左m0、右m1 | Hardware.cpp；3PWM d0=32/33/25/22，d1=26/27/14/12（前三PWM，末使能） |
| 编码器/IMU | AS5600双I2C；MPU6050在bus0 | bus0 SDA19/SCL18，bus1 SDA23/SCL5，均400kHz |
| 轮电压 | 驱动供电设8V，对齐6V，最终限幅±8V | Hardware.cpp、Safety.h；须与真实供电核对 |
| 驱动内置速度PID | P=.05 I=1 D=0 | Hardware.cpp；电机为voltage torque模式，不等于AI加了速度P环 |
| 舵机提交 | speed=400、acceleration=100，变化至少3计数才同步写 | Types.h、Hardware.cpp；单位为SCServo接口原生量，不误写成rad/s |
| 腿限位 | 左2047..2437，右1657..2047计数 | Safety.h，四舍五入后限幅 |
| 原始IMU | pitch=AngleY，roll=AngleX，gyro XYZ原轴 | Hardware.cpp；角度deg，角速度deg/s，accZ为库返回g |
| AI适配 | 角/角速度转rad/rad/s；双轮角、速、电压同时反号 | ai/Observation.h；不扣动态pitchZero，不交换轮通道；真实轴向/零位仍未标定 |
| 电池 | ADC1_CHANNEL_7，12bit/12dB，电压倍率3.075 | Hardware.cpp；8点均值，采样间隔5ms、批次500ms |
| 电量估计 | clamp((V-6.6)/1.5×100,0,100)% | 不是电池SOC标定；低于20%锁低电、高于25%解锁；超过2s视为过期 |

## 原生站立

`src/control/native/NativeController.h` 每个PID参数顺序为 P/I/D/output_ramp/limit：pitch=1/0/0/100000/8，gyro=.06/0/0/100000/8，distance=.5/0/0/100000/8，speed=.7/0/0/100000/8，yawAngle=1/0/0/100000/8，yawGyro=.04/0/0/100000/8，output=1/.5/0/100000/8，zero=.002/0/0/100000/4，roll=8/0/0/100000/450。

滤波时间常数：joy=.2s、zero=.1s、roll=.3s、height=.1s；初始height=30，目标50（沿用原控制量，不声明为mm）；pitchZero=-1.6deg且自适应限±10deg。速度差每100ms更新，abs(speedDiff)>10或abs(v)>15会重锚距离；输出小于5且距离项小于4才推进output/zero补偿。speed.P在本轮速度项后按height<50取.8、<64取.6、否则.5，作用于下一轮。此站立分支无摇杆、跳跃或任意高度运行时调节。

## AI合同与保护

- Actor单帧21维×三帧历史=63维；尺度/顺序见 `model/active-model.json` 的 actor_frame_names/actor_scales 与 FourCore.h。固件没有Critic，不能把训练侧65维Critic观测替换为Actor输入；当前合同的critic_privileged列表为空，不据此虚构额外通道。
- 动作顺序为左腿、右腿、左轮、右轮。动作裁剪±1；腿desired=clip(-.3297+.12a,-.5,-.12)rad；轮U=7.4a V，保持到下一次接纳，无轮LPF/P。
- 腿每1ms target最大改.003rad，estimate每步向desired靠近.025；estimate只是目标历史估计，不是舵机反馈。原零点左2299、右1795计数，4096计数/周，未完成带载标定。
- 合同控制步10ms、仿真步1ms、训练动作延迟7ms。固件同步推理，不人为再加7ms睡眠；1ms推进依据实际微秒差，超过100ms失败。10ms不是当前AI主循环强制定时器承诺。
- 进入AI：必须已在原生至少500ms，abs(pitch-nativePitchZero)<20deg，输入有效且种子成功；否则reason=NotStanding。
- `Safety.h/check`依次检查有限值、accZ>1.8g、有效电池、abs(pitch)>25deg或abs(roll)>35deg、elapsed>100000us。硬保护停机；控制期限/Actor失败/心跳失联在运行态且硬保护通过时交回原生。
- 非AI循环vTaskDelay(1)并补足一次至少1500us等待；不是固定1ms控制周期。看门狗3s，控制/网络任务均喂狗。

## 网络和运行时接口

| 配置 | 值/运行时方式 | 位置/影响 |
|---|---|---|
| 默认网络 | 本地secrets.h四宏 | 重编译；不要公开实际值 |
| 保存网络 | NVS namespace wifi_cfg；ssid/pass/static/ip/gw/sn/dns | Network.cpp只读已有值；当前网页没有保存配置接口，不提供虚构写入命令 |
| NVS优先级 | 有ssid键即优先，包括空值；static=true且ip/gw/sn解析成功才配置静态；dns无效取gw | 无效静态地址走DHCP；不自动擦除NVS |
| 工作模式 | WIFI_AP_STA，自动重连；断线15s主动重试 | AP与STA并行，默认STA为空则不启动 |
| HTTP | 80；GET /、/status、/diagnostics | 网络核0、优先级1、8192字节栈 |
| 控制权 | POST /claim；/heartbeat；/command含token和code | 运行时令牌随机生成，1500ms租约；活动所有者返回409，未授权返回403 |
| 命令码 | 0停止、1原生、2AI、3回原生、4舵机失能 | 命令最大年龄500ms；停止锁优先；排队失败锁存停止 |
| 网页节拍 | 心跳200ms、状态250ms（完成后调度），请求600ms超时；100ms检查状态，1s过期 | 两条独立单飞链，不以状态可读冒充所有权 |
| TCP日志 | 9871、单客户端、非阻塞，1500ms无发送进展断开 | 首行JSON+516字节帧；无认证加密，仅受信局域网使用 |
| 队列/事件 | 8遥测、8命令、32事件 | 遥测满只计drops；日志不做网络阻塞等待 |

HTTP令牌是局域网租约，不是TLS认证；项目没有HTTPS、用户管理或公网加固。不要直接暴露公网。网页及API只切换已编译控制器，不热加载模型，不提供PID调参界面。
