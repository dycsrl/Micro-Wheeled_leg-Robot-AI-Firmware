# 架构与阅读顺序

1. `src/app/main.cpp` 的中文 step 1..9 是控制主干。状态与控制器由控制核拥有；停止锁优先，非有限数据在进入有状态控制器前判断。候选输出再保护、限幅，最后唯一hardware::apply提交。
2. `hardware/Hardware.cpp` 同一翻译单元按原顺序构造相互依赖硬件对象。sample采集，apply提交；原生FOC/move顺序与AI先写q再FOC顺序保留。不要跨模块直接写电机对象。
3. `control/native` 独占PID和滤波状态；回切通过enter(resume=true)重锚时钟/距离，保留既有状态，不声称无扰交权。
4. `control/ai` 适配原始坐标，维护三帧历史及动作，调用生成Actor。FourCore不读硬件、不含Critic；观测、旧动作推进、推理完成、新动作接纳顺序不可重排。
5. `telemetry/Bridge` 是核间边界：固定队列传快照/命令，短临界区读状态/事件；控制路径零等待。网络只能消费副本，不改变已冻结帧，不运行控制计算。
6. `network/Network.cpp` 核0拥有Wi-Fi、NVS、HTTP、TCP。`Page.h` 以原始字符串保存展开HTML/JavaScript，可直接阅读；浏览器状态轮询和心跳互不串行阻塞。
7. `protocol/fields.def` 是C++/Python共享布局，Frame为516字节；tools/codec.py和parse.py离线解析。命令电压、估计腿角和真实传感器必须区分。

## 模型身份

`model/Actor.h` 是生成参数，保持原字节与active-model.json中actor_sha256一致；不要手动格式化。元数据中的checkpoint是来源相对标识，本源码不附该权重；不是运行时文件依赖。deployment/direct_contract.py检查确切合同和Actor身份，不代替冻结训练包审计。禁止部署标记保留，修改标记不等于获得实机验证。

## 本次整理边界

只在新副本展开自有C++/Python/网页排版、补中文说明；普通C++排版以去注释token比较，Python格式化以AST比较。测试中单行源码定位改为跨空白定位以适配展开格式，原断言保留。第三方供应源码和模型原字节保持。网页原先把失联提示写为“停止”，仅纠正文字为实际回原生行为，不改JavaScript控制语义。未改变控制、保护、模型数值、执行顺序或时序逻辑。离线桩与编译不代替真实硬件验收。
