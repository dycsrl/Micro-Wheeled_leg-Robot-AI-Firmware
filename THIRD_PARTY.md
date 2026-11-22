# 固件来源与许可

本副本来自本项目实际固件工作树；维护者确认自身修改和助手生成工作可以公开。这里不把整个工程改授MIT；原作者版权及对应许可继续适用。训练/机械资产未包含，不将那些资产的说明冒充本固件许可。

## 原机器人与部署参考

- 原机器人控制与硬件参考：[MuShibo/Micro-Wheeled_leg-Robot](https://github.com/MuShibo/Micro-Wheeled_leg-Robot)，贡献者穆世博（Mu Shibo / MuShibo）、李育锋（Li Yufeng）。已有来源核对未发现独立LICENSE，不凭公开可读推断全项目MIT；保留原作者声明与原使用条件，不为其增加或取消授权。
- 部署接口参考：[星际浪子 / mujoco_esp32_robot_deploy_public](https://gitee.com/starimpact_admin/mujoco_esp32_robot_deploy_public)，MIT，Copyright (c) 2024 星际浪子。已核对许可Git blob `03490848268b5583c8d4c46873ad5a955a054e92`，完整原文保存在 `licenses/mujoco_esp32_robot_deploy_public-LICENSE`；沿用/改编部分保留该版权与许可，不扩张覆盖其他组件。

## 供应库与补丁

| 组件 | 版本/来源事实 | 许可及说明 |
|---|---|---|
| Simple FOC | 2.2.1，对照47e1a7b42f7c0004006be674d9b988aa8a5b3083 | MIT，原LICENSE保留；pid.error_prev公开成员补丁及Lyf说明保留 |
| MPU6050_tockn | 1.5.2，对照c8e9dfb2ae37158b7ccb845eb1b1fd9f2197bf79 | tockn原README声明MIT；该tag无独立LICENSE，不虚构版权年份；校准后默认等待500ms补丁保留 |
| SCServo/SCSservo | FT&WS，1.0.1；参考workloads/scservo eb8700c84ac48abccd6f6925980eb56b66bb992b | 保留中文源与署名，附参考GPL-3.0全文lib/SCServo/LICENSE.txt；代码对照不单独证明精确下载来源 |

供应文件清单与SHA256在 dependencies.lock.json；本地补丁详见 lib/PATCHES.md。MIT部分保留完整版权及许可；SCServo参考GPL-3.0，分发相应衍生代码/二进制时遵守适用的源码提供、修改说明和许可条件，不改称全仓MIT。这些是现有来源事实与义务，不是新增逐一重新申请授权门槛。

## 构建工具

PlatformIO Core 6.1.18为外部构建工具；Arduino ESP32包声明LGPL-2.1-or-later及嵌套组件各自许可，Xtensa工具链包声明GPL-2.0-or-later并附组件例外。这些二进制没有复制入本源码。再分发工具/固件二进制时保留其发行版LICENSE/NOTICE，不能用本说明替代。
