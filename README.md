# C30D 四全向轮底盘固件

用于 STM32F407VE / C30D 控制板，支持前进、后退、左右横移和原地双向旋转。每个车轮使用编码器反馈和独立 PID 调速。
本仓库负责 STM32 固件；树莓派、视觉和 ROS2 程序不在本次修改范围内。

**当前状态：源码和主机逻辑测试已完成，尚未使用 Keil 编译生成新版 HEX，也没有实车验证。电机与编码器方向需要接线后校准。**

## 底盘与接线位置

按四个电机轴沿半径朝向车中心的全向轮底盘设计。车头取相邻两轮之间的方向，各轮主要滚动方向与车头约成 45°。

俯视小车：

```text
                  ↑ 车头 / FWD

          A 左前             B 右前
                     中心
          C 左后             D 右后
```

将各位置电机及其配套编码器连接到主板同名的 MotorA/B/C/D 接口。A/B/C/D 是本固件约定，接线时按此安排。

## 控制指令

USART1 或 USART3，115200 bps、8N1、无流控。发送 ASCII 文本，每条带换行 `\n`。

| 指令示例 | 动作 |
| --- | --- |
| `FWD,40` | 前进 |
| `BACK,40` | 后退 |
| `LEFT,40` | 向车身左侧横移 |
| `RIGHT,40` | 向车身右侧横移 |
| `CW,40` | 俯视顺时针原地旋转 |
| `CCW,40` | 俯视逆时针原地旋转 |
| `STOP` | 停止驱动 |
| `PING` | 测试通信，回复 `PONG` |

- 数字表示每轮目标转速的绝对值，范围 20～80 rpm；省略时默认 40 rpm。
- `RUN` / `RUN,40` 作为前进指令保留。
- 每 100 ms 重发当前运动指令。达到 500 ms 未收到有效运动或停止指令后，固件在控制周期检查时停止驱动。
- 上电默认停止；PD3 硬件使能关闭时不驱动电机。
- 状态自动上报，含运动方向、各轮目标转速、实际转速及 PWM。
- 本版不支持平移与旋转叠加，不支持逐轮独立指令。

四轮逻辑正方向统一取沿车中心逆时针的切线方向。因此前进时 A/C 为负、B/D 为正；四轮全正对应逆时针旋转。具体轮速表、引脚和校准方法见 [完整接线与协议说明](ROS_CONNECTION.md)。

## 编译和首次使用

1. 用 Keil ARM Compiler 5 打开 `Project.uvprojx`，选择 `C30D_STM32F407_4WD`。
2. 执行 Rebuild，生成 `Objects/C30D_4WD.hex`，再烧录到控制板。
3. 按上述位置接线，架空四轮，校准 `Hardware/BoardConfig.h` 中各轮的 `MOTOR_SIGN_*` 和 `ENCODER_SIGN_*`。方向系数目前均为待校准的 `+1` 初值。
4. 重新编译、烧录，架空低速验证六种动作、`STOP` 和超时停车，再进行落地测试。

`Baseline/C30D_4WD_original.hex` 是 CAR.zip 中保存的旧版固件，**不包含本次全向运动功能**。原压缩包解出的其他编译缓存也不能作为新版固件使用。

## 代码入口

| 路径 | 用途 |
| --- | --- |
| [User/main.c](User/main.c) | 运行状态、四轮 PID 调度、停车逻辑、显示 |
| [Hardware/Motion.c](Hardware/Motion.c) | 六种动作的轮速分配和运动指令解析 |
| [Hardware/ROSBridge.c](Hardware/ROSBridge.c) | 双串口接收、心跳记录、状态反馈 |
| [Hardware/BoardConfig.h](Hardware/BoardConfig.h) | 电机与编码器方向、编码器参数 |
| [ROS_CONNECTION.md](ROS_CONNECTION.md) | 接线、完整协议、轮速表和校准步骤 |
| [Tests/run.sh](Tests/run.sh) | Linux 主机逻辑测试 |

在仓库根目录运行：

```bash
./Tests/run.sh
```

测试覆盖轮速分配、指令解析与分帧、心跳计时、换向状态和反馈长度；不替代 ARM 编译和实车验证。`Tests/support` 中的 CMSIS shim 仅供主机测试，不能加入目标固件的头文件路径。

## Git 版本记录

本目录是独立的本地 Git 仓库。基准标签 `baseline-car-20261127` 保存 CAR.zip 导入时的版本，后续修改通过 Git 提交追踪。

```bash
git status       # 查看当前改动
git diff         # 查看尚未暂存的代码差异
git log --oneline # 查看版本记录
```
