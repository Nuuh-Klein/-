# 引脚分配与冲突核对

更新日期：2026-10-17。目标为 STM32F103C8。键盘与舵机已迁移，蜂鸣器同步迁移以消除 PB13 重复占用。以下是软件分配，需按表调整实物接线。

## 键盘接线顺序

严格按原 PA0～PA7 的物理接线顺序逐一替换，保留原行列及键值对应关系；原来的四行顺序为 R4、R3、R2、R1。

| 接线序号 | 原引脚 | 信号 | 新引脚 |
| --- | --- | --- | --- |
| 1 | PA0 | R4 | PB13 |
| 2 | PA1 | R3 | PB14 |
| 3 | PA2 | R2 | PB15 |
| 4 | PA3 | R1 | PA8 |
| 5 | PA4 | C1 | PA9 |
| 6 | PA5 | C2 | PA10 |
| 7 | PA6 | C3 | PA11 |
| 8 | PA7 | C4 | PA12 |

config.h 为每条线配置独立端口；key.c 按端口/引脚对逐行扫描。未选中行保持高阻，选中行使用开漏低电平，列使用上拉输入。防抖、长按不重复、多键锁定、键位 S1～S16、数字及 A/B/C/D/*/# 的功能保持原样。

## 当前全部分配

| 模块/信号 | 引脚 | 限制或状态 |
| --- | --- | --- |
| 键盘 R1/R2/R3/R4 | PA8/PB15/PB14/PB13 | 普通 GPIO，已支持跨端口，无 EXTI 占用 |
| 键盘 C1/C2/C3/C4 | PA9/PA10/PA11/PA12 | 普通 GPIO，不能同时启用这些脚上的串口或 USB 等复用功能 |
| AS608 MCU TX → 模块 RX | PA2 | **当前方案固定**：USART2 默认 TX |
| AS608 MCU RX ← 模块 TX | PA3 | **当前方案固定**：USART2 默认 RX |
| 舵机 PWM | PA0 | **通道受限**：TIM2_CH1 默认映射，已关闭 TIM2 重映射 |
| 编码器 A/CH1、B/CH2 | PB0、PB1 | GPIO + EXTI0/1，非定时器编码器模式 |
| 编码器按键 | PB10 | EXTI10，共用 EXTI15_10 中断入口 |
| 蜂鸣器 | **PB12（原 PB13）** | 原 PB13 已用于键盘 R4，蜂鸣器须同步改线；仍为低电平鸣叫 |
| 红灯、绿灯 | PC14、PC15 | 与 LSE 晶振互斥，存在输出速度和驱动电流限制 |
| OLED SCL、SDA | PB8、PB9 | 在 OLED.c 中定义，软件 I²C |

按此分配，应用 GPIO 无已知重复占用。AS608 原 PA2/PA3 与键盘的冲突已消除。PA15 不再输出舵机 PWM，驱动保留原先关闭 JTAG、保留 SWD 的调试配置；PA13/PA14 继续用于 SWD。

## 通道与硬件限制

- **AS608**：保留 USART2 时须使用 PA2/PA3。其重映射候选 PD5/PD6 在 C8 的 48 引脚封装未引出。模块并不要求主控必须使用 USART2，但更换串口须同步改接线、GPIO 和时钟驱动；AS608.c 将这些配置写死。当前无硬件流控，不占用舵机 PA0/CTS。
- **舵机**：仍为 TIM2_CH1，分频 72−1、周期 20000−1、关闭脉宽 500、开启脉宽 1500；按原 72 MHz 定时器时钟，周期为 20 ms，脉宽为 0.5/1.5 ms。默认 CH3/CH4 为 PA2/PA3，但未启用，不影响 USART2。motor.c 固定调用 CH1 接口，不能只改 PWM_TIM_CHANNEL 就更换通道。
- **编码器**：同号 EXTI 只能选择一个端口来源。键盘 PA10 使用轮询，不占 EXTI10，因此不与 PB10 按键中断冲突。迁移编码器须同步调整 GPIO、AFIO、EXTI、NVIC 和中断入口。
- **未来扩展**：键盘占用 PA9/PA10 后不能同时使用 USART1 默认收发；PA11/PA12 不能同时用于 USB；PB13～PB15 不能同时用于 SPI2；PB10 不能同时作为 USART3 默认 TX。

外设映射依据 [ST RM0008：GPIO/AFIO、定时器与 EXTI](https://www.st.com/resource/de/reference_manual/cd00171190.pdf)，封装依据 [ST DS5319：管脚定义](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)。

## 尚存的 LED 硬件注意事项

PC14/PC15 为 LSE 的 OSC32_IN/OSC32_OUT；若板上已接外部 32.768 kHz 晶振或需启用 LSE，应迁移 LED。当前应用代码未启用 LSE，实物晶振接线未知。

PC13～PC15 输出应遵守 3 mA、最大负载 30 pF、输出速度不超过 2 MHz 等限制，不得作为 LED 拉电流源。led.c 已修正为 GPIO_Speed_2MHz。低电平点亮仍需核对实物极性和限流电阻。[ST DS5319：Table 5 注 5 与输出驱动能力](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)。

## 验证与接入状态

main.c、key.c、OLED.c、motor.c、buzzer.c、led.c 已通过 Keil ARMCC 的 Cortex-M3 目标编译。tests/test_pin_migration.py 使用主机 GCC 和 GPIO 模拟运行键盘、舵机、蜂鸣器及 LED 实际驱动，验证键值、扫描隔离、消抖、多键处理、PWM 参数和两灯独立控制。尚未上板测试。

main.c 保留基础 IO 测试，数字 5 进入编码器/PB10 按键测试，物理 S14 分步控制 AS608 录入/识别，`*` 返回；原 main 业务代码已完整注释保留。完整步骤见 [外设测试操作](peripheral-tests.md)。编码器和 AS608 在进入各自模式时初始化。

S1～S4 使用现有驱动的物理位置编号（4 列行优先），即第一行 1/2/3/A：S1 红灯亮，S2 绿灯亮，S3 舵机转到约 90 度并保持，S4 蜂鸣器短响 50ms。灯独立保持亮灯；S3 再次按下仍保持 90 度。3×4 键盘仅有前三列时，第一行没有 S4/C4，数字 4 位于驱动位置 S5；若要使用数字 4 控制蜂鸣器，应按实际布局另行调整映射。
