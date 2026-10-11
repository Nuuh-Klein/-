#include "stm32f10x.h" // Device header
#include "Delay.h"
#include "OLED.h"
#include "config.h"
#include "store.h"
#include "log.h"
#include "buzzer.h"
#include "motor.h"
#include "encoder.h"
#include "led.h"
#include "key.h"
#include "AS608.h"

typedef enum{
    STATE_IDLE,    // 空闲状态：等待用户开始操作
    STATE_INPUT,   // 密码输入状态
    STATE_VERIFY,  // 密码校验状态
    STATE_LOCKED,  // 错误次数达到上限后的锁定状态
    STATE_ADMIN,   // 管理员功能状态
}STATE;

// 状态机及密码相关运行数据；set_password 和 duress_password 当前为示例默认值。
volatile uint16_t current_state=STATE_IDLE;           
volatile uint16_t error_count=0;
volatile int locking_remain=30;
volatile uint16_t origin_password[6]={0};
volatile uint16_t set_password[6]={1,1,1,1,1,1};
volatile uint16_t duress_password[6]={1,2,3,4,5,6};

int main(){
    // 初始化当前主程序已接入的显示、指示灯、蜂鸣器、舵机 PWM、密码存储和旋转编码器。
    // 键盘、指纹及日志模块目前尚未在此处初始化或调用。
    OLED_Init();
    LED_Init();
    Buzzer_Init();
    PWM_Init();
    Store_Init();
	  Encoder_Init();
   
    while(1){
        switch(current_state){
            case STATE_IDLE:{
                if(error_count==3){
                    // 错误次数达到阈值后，准备进入定时锁定。
                    
                    error_count=0; // 清零错误计数，等待下一轮错误统计。
                    locking_remain=LOCKING_TIME;
                    
                    OLED_ShowString(1,1,"WARING!!!");
                    OLED_ShowString(2,1,"waiting:");
                    
                    while(locking_remain){
                        // 显示剩余锁定时间；当前代码没有递减 locking_remain 的计时逻辑。
                        OLED_ShowNum(2,9,locking_remain,2);
                    }
                }
                OLED_ShowString(1,1,"press to start");
                // TODO：读取启动操作并切换到 STATE_INPUT。
                break;
            }
            case STATE_INPUT:{
                // TODO：读取并显示密码输入，输入完成后切换到 STATE_VERIFY。
                break;
            }
            case STATE_VERIFY:{
                uint8_t pass=1; // 先假定密码正确，再逐位比较。
                for(int i=0;i<6;i++){
                    if(origin_password[i]!=set_password[i]){
                        pass=0; // 任意一位不一致即判定失败。
                        current_state=STATE_IDLE;
                        error_count++;
                        break; // 无需继续比较后续位。
                    }
                }
                // TODO：校验失败时记录日志并反馈错误；区分普通密码与胁迫密码。
                
                if(pass){
                    // TODO：校验成功后记录开锁事件、反馈成功并调用 PWM_open()。
                }
                break;
            }
            case STATE_LOCKED:{
                if(locking_remain==0){
                    current_state=STATE_IDLE;
                }
                break;
            }
            case STATE_ADMIN:{
                OLED_ShowString(1,1,"ADMIN:");
                break;
            }
        }
    }
}
