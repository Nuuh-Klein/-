#include "stm32f10x.h" // Device header
#include "Delay.h"
#include "OLED.h"
#include "config.h"
#include "store.h"
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
	
	
	//初始化共享变量部分
	volatile uint16_t current_state=STATE_IDLE;
	volatile uint16_t error_count=0;
	volatile int locking_remain=30;
	volatile uint16_t origin_password[6]={0};
	volatile uint16_t set_password[6]={1,1,1,1,1,1};
	
	
	while(1){
		switch(current_state){
			case STATE_IDLE:{
				if(error_count==3){
					//蜂鸣器响
					error_count=3;
					locking_remain=LOCKING_TIME;
					OLED_ShowString(1,1,"WARING!!!");
					OLED_ShowString(2,1,"waiting:");
					OLED_ShowNum(2,9,locking_remain,2);
					while(locking_remain){
						//等待
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
				for(int i=0;i<6;i++){
					if(origin_password[i]!=set_password[i]){
						current_state=STATE_IDLE;
						error_count++;
						break;//密码输入错误
					}
				}
				//密码通过，触发电机，开锁
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
