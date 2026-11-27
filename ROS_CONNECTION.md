# 第五次任务电控与 ROS2 连接说明

本工程只实现电控与嵌入式部分。视觉程序和 ROS2 程序由其他方向负责，通过串口向本工程发送运行或停止指令。

## 1. 供电连接

使用 C30D 左侧蓝色电源输入端子：

1. 输入电源范围按主板标识为 6～16 V。
2. 建议使用 12 V 电机电源，正极接电源输入正极，负极接 GND。
3. 打开主板左侧的主开关。
4. 打开电机使能开关 PD3。
5. 不要在程序启动阶段发送 `RUN`，STM32 上电默认保持停止。

板载稳压电路会由该电源输入产生 MCU、外设和电机驱动所需的电源。USB-C 主要用于串口通信和程序下载，不是电机电源。

## 2. 推荐通信连接

C30D 标注串口 3 用于与树莓派或 Jetson 等 ROS 主机通信。优先使用板上串口 3 接口连接 ROS 主机。

如果使用板载串口 3 的 USB 转换接口：

1. 使用数据 USB 线连接 C30D 串口 3 和 ROS 主机。
2. Linux 下检查设备：

```bash
ls -l /dev/serial/by-id/
dmesg | tail -20
```

常见设备名为 `/dev/ttyUSB0`，具体以实际设备为准。

3. 确认串口权限：

```bash
sudo usermod -aG dialout "$USER"
```

重新登录后生效。

如果直接使用 USART3 TTL 引脚：

| C30D | ROS 主机 |
| --- | --- |
| PD8 / USART3_TX | RXD |
| PD9 / USART3_RX | TXD |
| GND | GND |

只能连接 3.3 V TTL 串口，不要接 5 V TTL。使用板载 USB 串口时不需要额外连接这三根线。

## 3. 串口参数

```text
115200 bps
8 data bits
no parity
1 stop bit
no flow control
ASCII text with newline
```

固件同时监听 USART1 和 USART3。实际联调建议只使用串口 3，避免两个主机同时控制。

## 4. ROS2 发送规则

上位机每 100 ms 发送一次当前请求，不能只在状态变化时发送一次：

有目标：

```text
RUN,40
```

无目标：

```text
STOP
```

也支持 `RUN`，此时默认目标为 40 rpm。有效目标范围为 20～80 rpm。

如果 500 ms 内没有收到有效命令，STM32 自动停车，OLED 显示 `TMO`，反馈原因 `TIMEOUT`。

## 5. STM32 状态反馈

启动后通过两个串口发送：

```text
READY,PROTOCOL=MOTOR_4WD_V1,DRIVE=4WD,UART1+UART3=115200,HEARTBEAT_TIMEOUT_MS=500
```

运行期间每 100 ms 发送：

```text
STATUS,SEQ=18,STATE=RUN,CMD=RUN,DRIVE=4WD,TARGET_RPM=40,ACTUAL_RPM=39,RPM_A=40,RPM_B=38,RPM_C=39,RPM_D=39,PWM_A=1250,PWM_B=1310,PWM_C=1240,PWM_D=1270,AGE_MS=47,VALID=1,REASON=ROS_RUN
```

关键字段：

| 字段 | 含义 |
| --- | --- |
| `STATE` | STM32 当前运行状态 |
| `TARGET_RPM` | 当前目标转速 |
| `ACTUAL_RPM` | 四轮平均转速 |
| `RPM_A`～`RPM_D` | 四轮实际转速 |
| `PWM_A`～`PWM_D` | 四轮 PWM 输出 |
| `AGE_MS` | 距最近有效 ROS 指令的时间 |
| `VALID` | 是否收到过有效指令 |
| `REASON` | `ROS_RUN`、`ROS_STOP`、`TIMEOUT`、`HW_DISABLED` 或 `BOOT` |

## 6. ROS2 与视觉程序的对应关系

视觉程序输出有目标和无目标两种结果。ROS2 通信节点按以下关系转换：

| 视觉结果 | ROS2 串口命令 |
| --- | --- |
| 有指定颜色目标 | `RUN,40\n` |
| 无目标或目标移出画面 | `STOP\n` |
| 启动后尚未收到视觉结果 | `STOP\n` |
| 视觉消息超时 | `STOP\n` |

ROS2 节点在启动后等待第一条视觉消息，等待期间持续发送 `STOP`。收到视觉消息后，每 100 ms 根据最新结果发送对应命令。

## 7. 联调检查顺序

1. 只接电源输入，不接电机，打开主开关，确认 OLED 点亮。
2. 连接串口 3，确认收到 `READY`。
3. 发送 `PING`，确认收到 `PONG`。
4. 发送 `STOP`，确认 OLED 为 `4WD STP`。
5. 架空四轮后持续发送 `RUN,40`，确认四轮运行并收到 `STATE=RUN`。
6. 停止发送 `RUN`，确认约 500 ms 后四轮停止，反馈变为 `REASON=TIMEOUT`。
7. 启动 ROS2 和视觉程序，确认有目标时四轮运行、无目标时四轮停止。
