#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"

void KEY_Test_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	//行 PA0~PA3 推挽输出
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_SetBits(GPIOA, GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3);
	//列 PA4~PA7 上拉输入
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
}

int main(){
    //硬件初始化部分
    OLED_Init();
    LED_Init();
    Buzzer_Init();
    PWM_Init();
    Store_Init();
	  Encoder_Init();
    //初始化共享变量部分
    volatile uint16_t current_state=STATE_IDLE;           
    volatile uint16_t error_count=0;
    volatile int locking_remain=30;
    volatile uint16_t origin_password[6]={0};
    volatile uint16_t set_password[6]={1,1,1,1,1,1};
    volatile uint16_t duress_password[6]={01,2,3,4,5,6};
    
    
    while(1){
        switch(current_state){
            case STATE_IDLE:{
                if(error_count==3){
                    //蜂鸣器响
                    
                    error_count=0;//错误计次归零
                    locking_remain=LOCKING_TIME;
                    
                    OLED_ShowString(1,1,"WARING!!!");
                    OLED_ShowString(2,1,"waiting:");
                    
                    while(locking_remain){
                        //等待倒计时结束
                        OLED_ShowNum(2,9,locking_remain,2);
                    }
                }
                OLED_ShowString(1,1,"press to start");
                //
                break;
            }
            case STATE_INPUT:{
                //
                break;
            }
            case STATE_VERIFY:{
                uint8_t pass=1;                 //先假定校验通过
                for(int i=0;i<6;i++){
                    if(origin_password[i]!=set_password[i]){
                        pass=0;                     //任意一位不匹配即失败
                        current_state=STATE_IDLE;
                        error_count++;
                        break;//密码输入错误
                    }
                }
                //无论成功还是失败，都记入开锁审计日志（创意四）
                
                if(pass){
                    //密码通过，触发电机，开锁（TODO：调用 PWM_open() 完成开锁）
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
