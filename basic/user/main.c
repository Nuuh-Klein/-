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
	STATE_IDLE,    //待机状态
	STATE_INPUT,   //输入密码
	STATE_VERIFY,  //校验密码
	STATE_LOCKED,  //锁定状态
	STATE_ADMIN,   //管理员模式
}STATE;

int main(){
	//硬件初始化部分
	OLED_Init();
	LED_Init();
	Buzzer_Init();
	PWM_Init();
	Store_Init();
	Log_Init();
	
	//初始化共享变量部分
	volatile uint16_t current_state=STATE_IDLE;           
	volatile uint16_t error_count=0;
	volatile int locking_remain=30;
	volatile uint16_t origin_password[6]={0};
	volatile uint16_t set_password[6]={1,1,1,1,1,1};
	volatile uint16_t duress_password[6]={1,2,3,4,5,6};
	volatile uint16_t duress_flag=0;            //胁迫标记：0=正常，1=曾输入胁迫密码
	
	
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
				//第一层：先比对正常密码 set_password
				uint8_t pass=1;
				for(int i=0;i<6;i++){
					if(input_password[i]!=set_password[i]){
						pass=0;
						break;
					}
				}

				if(pass){
					//正常密码正确：记日志并开锁
					Log_Write(LOG_METHOD_PASSWORD, LOG_RESULT_PASS);
					//密码通过，触发电机，开锁（TODO：调用 PWM_open() 完成开锁）

					//若此前被胁迫过，开锁后悄悄提示主人一次，提示完清零
					if(duress_flag){
						OLED_ShowString(1,1,"DURESS ALERT!");
						OLED_ShowString(2,1,"been forced");
						Delay_ms(2000);
						duress_flag=0;
					}
				}
				else{
					//第二层：正常密码不对，再比胁迫密码 duress_password
					uint8_t duress=1;
					for(int i=0;i<6;i++){
						if(input_password[i]!=duress_password[i]){
							duress=0;
							break;
						}
					}

					if(duress){
						//胁迫密码正确：照常开锁迷惑胁迫者，但静默告警（只记日志+置标记）
						duress_flag=1;
						Log_Write(LOG_METHOD_DURESS, LOG_RESULT_PASS);
						//照常开锁（TODO：调用 PWM_open() 完成开锁）
					}
					else{
						//两套密码都不对：记一次失败
						error_count++;
						Log_Write(LOG_METHOD_PASSWORD, LOG_RESULT_FAIL);
						current_state=STATE_IDLE;
					}
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
