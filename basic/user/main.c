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
    STATE_IDLE,    //????
    STATE_INPUT,   //????
    STATE_VERIFY,  //????
    STATE_LOCKED,  //????
    STATE_ADMIN,   //?????
}STATE;

 //?????????
volatile uint16_t current_state=STATE_IDLE;           
volatile uint16_t error_count=0;
volatile int locking_remain=30;
volatile uint16_t origin_password[6]={0};
volatile uint16_t set_password[6]={1,1,1,1,1,1};
volatile uint16_t duress_password[6]={1,2,3,4,5,6};

int main(){
    //???????
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
                    //????
                    
                    error_count=0;//??????
                    locking_remain=LOCKING_TIME;
                    
                    OLED_ShowString(1,1,"WARING!!!");
                    OLED_ShowString(2,1,"waiting:");
                    
                    while(locking_remain){
                        //???????
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
                uint8_t pass=1;                 //???????
                for(int i=0;i<6;i++){
                    if(origin_password[i]!=set_password[i]){
                        pass=0;                     //??????????
                        current_state=STATE_IDLE;
                        error_count++;
                        break;//??????
                    }
                }
                //????????,?????????(???)
                
                if(pass){
                    //????,????,??(TODO:?? PWM_open() ????)
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
