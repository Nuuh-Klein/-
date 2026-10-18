# 外设验证记录与保留实现

更新：2026-10-18。用户已确认键盘/OLED、S1～S4控制外设测试正常，JM101指纹录入和比对通过。main已恢复测试前保留的原业务状态机；临时测试界面及串口诊断源文件已删除。

## 保留的配置与修正

- 键盘八根线按原物理顺序迁移为PB13、PB14、PB15、PA8、PA9、PA10、PA11、PA12，保持原键值，保留消抖、长按不重复与多键隔离。
- OLED继续使用PB8/PB9；舵机TIM2_CH1/PA0；蜂鸣器PB12，避免与键盘共用PB13；LED为PC14/PC15，输出速度2MHz。
- 编码器保留PB0/PB1的EXTI正交解码及PB10按键接口，中断内不做延时。
- 指纹模块TX接PA3、RX接PA2，稳定3.3V供电、共地；保留已通过实测的USART2、57600/8N2、默认地址FFFFFFFF和默认密码0。
- USART2接收中断先收取原始字节并放入128槽环形缓存，主程序再解析完整报文和校验和。保留错误统计、丢字节统计、超时与取消处理。
- JM101正式驱动接口：AS608_Init、AS608_VerifyPassword、AS608_ReadTemplateCount、AS608_JM101Enroll、AS608_JM101Search。进度回调和取消回调保留，便于后续业务调用。录入禁止覆盖已有ID和重复指纹，仅收到最终存储成功应答才返回成功；比对返回匹配ID及分数。

## 指纹超时排查结论

最初cmd13超时；隔离接线和外设后仍未解决。USB转TTL独立直连模块，在57600/8N2下收到合法成功应答，确认模块可以通信。STM32出现RX001/E08，记录到接收溢出；改为中断缓存接收后用户确认通信成功，随后确认录入/比对测试通过。

接收溢出及修复后的实测是本次故障分析的主要证据；供电不足、OLED造成丢包等没有被单独证明为根因。初始化保留明确的PA2/PA3映射，不再发送该JM101手册未列出的可选灯控命令。

## 当前业务状态

main恢复原STATE_IDLE、STATE_INPUT、STATE_VERIFY、STATE_LOCKED、STATE_ADMIN分支及原有初始化，指纹和键盘尚未接入业务状态机。原有待实现项（密码输入、状态切换、锁定倒计时、开锁反馈等）保持原样。本次恢复不等于完整密码锁业务已实现。

仅供硬件交互的S14测试流程、S1～S4快捷键、波特率扫描、OLED诊断页和PA2/PA3回环入口均已清理。主机回归测试位于tests目录，不编入单片机固件，保留用于检查实际驱动。

## 构建与检查

```text
python -B tests/test_pin_migration.py
python -B tests/test_extended_peripherals.py
python tools/build_firmware.py
```

正常Keil工程为basic/led.uvprojx，默认下载产物为basic/Objects/led.axf及led.hex。通用构建脚本生成同内容的basic/build/locking.hex、AXF、MAP和源码散列清单，Flash链接范围避开0x0800F400起的日志/密码保留区。旧测试固件已移除。
