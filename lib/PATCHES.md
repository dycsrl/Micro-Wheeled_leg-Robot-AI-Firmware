# 供应源码与上游差异

这些源码来自原固件的本地lib目录，迁移时逐字节保留，避免安装同版本号的原版库丢失补丁。清单和SHA256见`dependencies.lock.json`。

- Simple FOC 2.2.1：对照上游`47e1a7b42f7c0004006be674d9b988aa8a5b3083`，仅`src/common/pid.h`改变：将`error_prev`从protected移至public，原有Lyf说明保留。MIT，原LICENSE完整保留。
- MPU6050_tockn 1.5.2：对照上游`c8e9dfb2ae37158b7ccb845eb1b1fd9f2197bf79`，校准后默认等待3000ms改为500ms，串口信息改为显示实际delayAfter。MIT声明与作者tockn在原README保留；该tag未提供独立LICENSE文件，按原README保留MIT声明和作者信息，不虚构版权年份。
- SCServo/SCSservo 1.0.1：保留FT&WS元数据和中文源注释。对照workloads/scservo 1.0.1 (`eb8700c84ac48abccd6f6925980eb56b66bb992b`) 的英文注释/元数据版本；参考上游GPL-3.0全文置于该库LICENSE.txt。该对照不单独证明本地快照的精确下载来源；保留FT&WS署名、参考GPL全文和此来源事实。分发时遵守适用的GPL源码提供、修改说明和许可要求，不对全仓套MIT，不将来源记录写成重新向作者申请许可的门槛。

完整源码用于保留各库自身平台条件编译闭包；示例、测试、图片和缓存不迁移。
