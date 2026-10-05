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
	
	buzzer2();
	while(1){
		
   
	}
}
