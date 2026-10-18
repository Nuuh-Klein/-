#ifndef __CONFIG_H 
#define __CONFIG_H  

#include "stm32f10x.h"                  // Device header

/* 引脚核对（STM32F103C8；完整分配表见 docs/pin-audit.md）：
 * [已迁移] 原键盘 PA0~PA7 按接线顺序迁至 PB13/PB14/PB15/PA8/PA9/PA10/PA11/PA12。
 * [固定] AS608 保留 USART2 的 PA2/PA3；蜂鸣器由 PB13 迁至 PB12，避免与键盘共用。
 * [通道受限] 舵机使用 TIM2_CH1/PA0 默认映射；编码器使用 EXTI0/1，按键使用 EXTI10。
 * [硬件受限] PC14/PC15 与 LSE 晶振互斥，且输出驱动能力受限。
 * [config 外占用] OLED 在 OLED.c 中固定写为 PB8/PB9，分配新引脚时也须避开。
 * [接入状态] main.c 已恢复原业务状态机；JM101驱动保留，尚未接入业务流程。
 */

//密码参数
#define PASSWORD_LEN 6        //密码长度为6位
#define MAX_ERROR_COUNT 3     //最长错误次数3次
#define LOCKING_TIME 30       //锁定时长为30秒
// [可迁移] 4×4 矩阵键盘：每条线独立配置端口，支持跨 GPIOA/GPIOB 扫描。
// 保持原 PA0~PA7 的接线顺序：R4/R3/R2/R1/C1/C2/C3/C4，不改变键位和键值。
// 新接线依次为 PB13/PB14/PB15/PA8/PA9/PA10/PA11/PA12。
#define KEY_ROW_RCC     (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB)
#define R1_PORT         GPIOA
#define R1_PIN          GPIO_Pin_8
#define R2_PORT         GPIOB
#define R2_PIN          GPIO_Pin_15
#define R3_PORT         GPIOB
#define R3_PIN          GPIO_Pin_14
#define R4_PORT         GPIOB
#define R4_PIN          GPIO_Pin_13

#define KEY_COL_RCC     RCC_APB2Periph_GPIOA
#define C1_PORT         GPIOA
#define C1_PIN          GPIO_Pin_9
#define C2_PORT         GPIOA
#define C2_PIN          GPIO_Pin_10
#define C3_PORT         GPIOA
#define C3_PIN          GPIO_Pin_11
#define C4_PORT         GPIOA
#define C4_PIN          GPIO_Pin_12
#define PWD_MAX_LEN	    6	//密码长度固定6位


// [可迁移/硬件受限] LED：PC14/PC15 不能与外部 32.768 kHz LSE 晶振同时使用。
// PC13~PC15 输出速度应 <=2MHz，并遵守 3mA/30pF 限制，不可用作 LED 拉电流源。
// led.c 已配置为 2MHz；当前低电平点亮仍须核对实物限流/晶振电路。
// 当前 LED_Init 一次初始化两灯，迁移时须让两灯同端口，或同步修改初始化代码。
#define LED_RCC_PORT        RCC_APB2Periph_GPIOC
#define LED_Green_PORT            GPIOC
#define LED_Green_Pin       GPIO_Pin_15
#define LED_Red_PORT        GPIOC
#define LED_Red_Pin         GPIO_Pin_14

// [EXTI 受限/可迁移] 编码器采用 GPIO + EXTI 解码，并非 TIM3 硬件编码器模式。
// CH1/CH2 是相位名称，不是定时器通道；PB0/PB1 分别占 EXTI0/EXTI1。
// 同号 EXTI 只能选择一个 GPIO 端口；改引脚须同步改 AFIO/EXTI/NVIC/IRQHandler。
// 当前驱动用同一 ENCODER_PORT 读取两相，迁移时两相须保持同端口，或修改驱动。
#define ENCODER_RCC_PORT    RCC_APB2Periph_GPIOB
#define ENCODER_PORT        GPIOB
#define ENCODER_Pin_CH1     GPIO_Pin_0
#define ENCODER_Pin_CH2     GPIO_Pin_1

//AFIO映射宏（给GPIO_EXTILineConfig专用）
#define ENCODER_PORTSOURCE      GPIO_PortSourceGPIOB    //AFIO端口源
#define ENCODER_PINSOURCE_CH1   GPIO_PinSource0         //AFIO引脚源 CH1 PB0
#define ENCODER_PINSOURCE_CH2   GPIO_PinSource1         //AFIO引脚源 CH2 PB1

//EXTI中断线
#define ENCODER_EXTI_LINE_CH1   EXTI_Line0
#define ENCODER_EXTI_LINE_CH2   EXTI_Line1
// [EXTI 受限/可迁移] 编码器旋钮按键：PB10 -> EXTI10，共用 EXTI15_10 IRQ。
// PB10 同时是 USART3 默认 TX、TIM2 全重映射 CH3 的候选脚；当前未启用这些功能，无冲突。
#define ENC_KEY_PORT			GPIOB
#define ENC_KEY_RCC_PORT		RCC_APB2Periph_GPIOB
#define ENC_KEY_PIN				GPIO_Pin_10
#define ENC_KEY_PORTSOURCE		GPIO_PortSourceGPIOB
#define ENC_KEY_PINSOURCE		GPIO_PinSource10
#define ENC_KEY_EXTI_LINE		EXTI_Line10
#define ENC_KEY_IRQn			EXTI15_10_IRQn

//AFIO时钟
#define ENCODER_AFIO_RCC        RCC_APB2Periph_AFIO

// [可迁移] 蜂鸣器由普通 GPIO 高低电平控制，当前不占 PWM/定时器通道。
// 原 PB13 已分配键盘 R4，蜂鸣器接线同步迁至 PB12，保持低电平鸣叫。
#define BUZZER_GPIO_PORT   GPIOB
#define BUZZER_GPIO_PIN    GPIO_Pin_12
#define BUZZER_GPIO_CLK    RCC_APB2Periph_GPIOB

#define BUZZER_BEEP_MS     50   
#define BUZZER_PAUSE_MS    50    
#define BUZZER_ALARM_TIMES 20    


// [通道受限] 舵机 PWM：TIM2_CH1 默认映射 PA0；该通道仅可选 PA0 或 PA15。
// 键盘已迁移，PA0 专用于舵机；初始化时关闭 TIM2 重映射，PA15 不再输出 PWM。
// motor.c 写死 TIM_OC1Init/SetCompare1，PWM_TIM_CHANNEL 宏当前未被驱动使用。
// 更换通道不能只改 PWM_GPIO_PIN/PWM_TIM_CHANNEL，还须同步改驱动和重映射。
#define PWM_TIM                 TIM2
#define PWM_TIM_CLK             RCC_APB1Periph_TIM2

#define PWM_TIM_CHANNEL         TIM_Channel_1 

#define PWM_GPIO_PORT           GPIOA
#define PWM_GPIO_PIN            GPIO_Pin_0
#define PWM_GPIO_CLK            RCC_APB2Periph_GPIOA
 
#define PWM_AFIO_CLK            RCC_APB2Periph_AFIO
#define PWM_REMAP_PIN           GPIO_FullRemap_TIM2
#define PWM_REMAP_STATE         DISABLE  // 清除 TIM2_REMAP 两位，使用 PA0 默认映射

#define PWM_TIM_PRESCALER       (72 - 1)        // PSC
#define PWM_TIM_PERIOD          (20000 - 1)     // ARR

#define PWM_PULSE_0_DEG         500             //close
#define PWM_PULSE_90_DEG        1500            //open

// [当前方案固定] AS608：USART2，PA2 = MCU TX -> 模块 RX；PA3 = MCU RX <- 模块 TX。
// 键盘已迁移，PA2/PA3 不再与键盘共用；USART2 无硬件流控，不占舵机 PA0/CTS。
// USART2 重映射的 PD5/PD6 在 C8 的 48 引脚封装未引出，不能靠重映射迁移 USART2。
// AS608 模块本身不指定 MCU 管脚；换 USART1/USART3 可另行设计，但须改接线和驱动。
// AS608.c 将 GPIOA/PA2/PA3 和 USART2 时钟写死，仅修改 AS608_USART 宏无效。
// TIM2 默认 CH3/CH4 映射 PA2/PA3，但当前仅启用 CH1，不构成额外冲突。
#define AS608_USART            USART2
#define AS608_BAUDRATE         57600
#define AS608_STOP_BITS        USART_StopBits_2 /* JM101实机验证通过：57600/8N2 */
#define AS608_DEVICE_ADDRESS   0xFFFFFFFFUL /* 主机报文地址；不修改模块地址 */
#define AS608_POWER_ON_DELAY   500U
#define AS608_PASSWORD         0x00000000UL /* 默认通信密码；不修改模块中的密码 */
#define AS608_PACKET_TYPE_CMD  0x01
#define AS608_PACKET_TYPE_ACK  0x07
#define AS608_RX_BUFFER_SIZE   64 /* 索引表 ACK 共44字节，原32字节缓冲区不足 */
#define AS608_DEFAULT_TIMEOUT  1500U /* milliseconds to wait for the first response byte */
#define AS608_INTERBYTE_TIMEOUT 100U  /* maximum gap between bytes of one ACK packet */
#define AS608_FINGER_WAIT_TIMEOUT 30000U /* bounded wait for placement/removal */

//密码存储参数分配
#define Store_Start_Address   0x0800FC00     //存放密码的flash地址（第63页）
#define Store_Data_Length     12             //存放密码的数组长度

//审计日志存储参数分配（创意四：开锁审计日志）
#define LOG_START_ADDRESS   0x0800F400                                        //日志区起始地址（第61页）
#define LOG_PAGE_SIZE       1024                                              //每页1KB
#define LOG_PAGE_COUNT      2                                                 //日志区占用2页，共2KB
#define LOG_RECORD_SIZE     6                                                 //每条日志定长6字节=3个半字{方式,成败,序号}
#define LOG_MAX_RECORDS     ((LOG_PAGE_SIZE*LOG_PAGE_COUNT)/LOG_RECORD_SIZE)  //1024*2/6==最多341条（末尾2字节不用）

//FLASH宏定义
#define USER_FLASH_BASE_ADDRESS 0x08000000UL
#define USER_FLASH_END_ADDRESS  0x08010000UL
#define USER_FLASH_PAGE_SIZE    1024UL

//共享变量声明
extern volatile uint16_t current_state;          //状态机目前的状态
extern volatile uint16_t error_count;            //输入错误的次数
extern volatile int locking_remain;              //锁定剩余的时长
extern volatile uint16_t origin_password[6];     //初始密码
extern volatile uint16_t set_password[6];        //设置密码
extern volatile uint16_t duress_password[6];     //胁迫密码

#endif
