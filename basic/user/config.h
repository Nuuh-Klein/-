#ifndef __CONFIG_H 
#define __CONFIG_H  

#include "stm32f10x.h"                  // Device header

//密码参数
#define PASSWORD_LEN 6        //密码长度为6位
#define MAX_ERROR_COUNT 3     //最长错误次数3次
#define LOCKING_TIME 30       //锁定时长为30秒



//LED参数分配（端口/引脚）
#define LED_RCC_PORT        RCC_APB2Periph_GPIOC
#define LED_PORT            GPIOC
#define LED_Green_Pin       GPIO_Pin_15
#define LED_Red_PORT        GPIOC
#define LED_Red_Pin         GPIO_Pin_1

//编码器参数分配
#define ENCODER_RCC_PORT    RCC_APB2Periph_GPIOB
#define ENCODER_PORT        GPIOB
#define ENCODER_Pin_CH1     GPIO_Pin_0
#define ENCODER_Pin_CH2     GPIO_Pin_1

//====AFIO映射宏（给GPIO_EXTILineConfig专用）====
#define ENCODER_PORTSOURCE      GPIO_PortSourceGPIOB    //AFIO端口源
#define ENCODER_PINSOURCE_CH1   GPIO_PinSource0         //AFIO引脚源 CH1 PB0
#define ENCODER_PINSOURCE_CH2   GPIO_PinSource1         //AFIO引脚源 CH2 PB1

//====EXTI中断线====
#define ENCODER_EXTI_LINE_CH1   EXTI_Line0
#define ENCODER_EXTI_LINE_CH2   EXTI_Line1
//====编码器旋钮按键（按下返回初始界面）====
#define ENC_KEY_PORT			GPIOB
#define ENC_KEY_RCC_PORT		RCC_APB2Periph_GPIOB
#define ENC_KEY_PIN				GPIO_Pin_10
#define ENC_KEY_PORTSOURCE		GPIO_PortSourceGPIOB
#define ENC_KEY_PINSOURCE		GPIO_PinSource10
#define ENC_KEY_EXTI_LINE		EXTI_Line10
#define ENC_KEY_IRQn			EXTI15_10_IRQn

//====AFIO时钟宏====
#define ENCODER_AFIO_RCC        RCC_APB2Periph_AFIO

//蜂鸣器参数分配

#define BUZZER_GPIO_PORT   GPIOB
#define BUZZER_GPIO_PIN    GPIO_Pin_13
#define BUZZER_GPIO_CLK    RCC_APB2Periph_GPIOB

#define BUZZER_BEEP_MS     50   
#define BUZZER_PAUSE_MS    50    
#define BUZZER_ALARM_TIMES 20    

//定时器参数分配


//发动机参数分配
#define PWM_TIM                 TIM2
#define PWM_TIM_CLK             RCC_APB1Periph_TIM2

#define PWM_TIM_CHANNEL         TIM_Channel_1 

#define PWM_GPIO_PORT           GPIOA
#define PWM_GPIO_PIN            GPIO_Pin_15
#define PWM_GPIO_CLK            RCC_APB2Periph_GPIOA
 
#define PWM_AFIO_CLK            RCC_APB2Periph_AFIO
#define PWM_REMAP_PIN           GPIO_PartialRemap1_TIM2

#define PWM_TIM_PRESCALER       (72 - 1)        // PSC
#define PWM_TIM_PERIOD          (20000 - 1)     // ARR

#define PWM_PULSE_0_DEG         500             //close
#define PWM_PULSE_90_DEG        1500            //open



//密码存储参数分配
#define Store_Start_Address   0x0800FC00     //存放密码的flash地址（第63页）
#define Store_Data_Length     12             //存放密码的数组长度

//审计日志存储参数分配（创意四：开锁审计日志）
#define LOG_START_ADDRESS   0x0800F400                                        //日志区起始地址（第61页）
#define LOG_PAGE_SIZE       1024                                              //每页1KB
#define LOG_PAGE_COUNT      2                                                 //日志区占用2页，共2KB
#define LOG_RECORD_SIZE     6                                                 //每条日志定长6字节=3个半字{方式,成败,序号}
#define LOG_MAX_RECORDS     ((LOG_PAGE_SIZE*LOG_PAGE_COUNT)/LOG_RECORD_SIZE)  //1024*2/6==最多341条（末尾2字节不用）

//共享变量声明
extern volatile uint16_t current_state;          //状态机目前的状态
extern volatile uint16_t error_count;            //输入错误的次数
extern volatile int locking_remain;              //锁定剩余的时长
extern volatile uint16_t origin_password[6];     //初始密码
extern volatile uint16_t set_password[6];        //设置密码
extern volatile uint16_t duress_password[6];     //胁迫密码

#endif
