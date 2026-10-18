"""Host tests of actual encoder, AS608 protocol and main test-mode helpers."""
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_pin_migration import ROOT, STUB

EXTRA = r"""
typedef enum {RESET, SET} FlagStatus;
typedef FlagStatus ITStatus;
typedef unsigned FLASH_Status;
#define EXTI_Line0 (1U<<0)
#define EXTI_Line1 (1U<<1)
#define EXTI_Line10 (1U<<10)
#define EXTI0_IRQn 6U
#define EXTI1_IRQn 7U
#define EXTI15_10_IRQn 40U
#define GPIO_PortSourceGPIOB 1U
#define GPIO_PinSource0 0U
#define GPIO_PinSource1 1U
#define GPIO_PinSource10 10U
#define EXTI_Mode_Interrupt 0U
#define EXTI_Trigger_Rising_Falling 3U
#define EXTI_Trigger_Falling 2U
typedef struct {unsigned EXTI_Line, EXTI_LineCmd, EXTI_Mode, EXTI_Trigger;} EXTI_InitTypeDef;
typedef struct {
    unsigned NVIC_IRQChannel, NVIC_IRQChannelCmd;
    unsigned NVIC_IRQChannelPreemptionPriority, NVIC_IRQChannelSubPriority;
} NVIC_InitTypeDef;
void GPIO_EXTILineConfig(uint8_t, uint8_t);
void EXTI_Init(EXTI_InitTypeDef *);
void EXTI_ClearITPendingBit(uint32_t);
ITStatus EXTI_GetITStatus(uint32_t);
void NVIC_Init(NVIC_InitTypeDef *);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
typedef struct {uint32_t CTRL, LOAD, VAL;} SysTick_Type;
extern SysTick_Type mock_systick;
#define SysTick (&mock_systick)
#define SysTick_CTRL_CLKSOURCE_Msk 4U
#define SysTick_CTRL_ENABLE_Msk 1U
#define SysTick_CTRL_COUNTFLAG_Msk (1U<<16)
extern uint32_t SystemCoreClock;
typedef struct {unsigned unused;} USART_TypeDef;
extern USART_TypeDef usart2;
#define USART2 (&usart2)
#define USART_FLAG_TXE 1U
#define USART_FLAG_RXNE 2U
#define RCC_APB1Periph_USART2 32U
#define USART_WordLength_8b 8U
#define USART_StopBits_1 1U
#define USART_Parity_No 0U
#define USART_HardwareFlowControl_None 0U
#define USART_Mode_Rx 1U
#define USART_Mode_Tx 2U
typedef struct {
    unsigned USART_BaudRate, USART_WordLength, USART_StopBits, USART_Parity;
    unsigned USART_HardwareFlowControl, USART_Mode;
} USART_InitTypeDef;
FlagStatus USART_GetFlagStatus(USART_TypeDef *, uint16_t);
void USART_SendData(USART_TypeDef *, uint16_t);
uint16_t USART_ReceiveData(USART_TypeDef *);
void USART_Init(USART_TypeDef *, USART_InitTypeDef *);
void USART_Cmd(USART_TypeDef *, FunctionalState);
"""

MAIN_TEST = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define main firmware_main
#include "MAIN_SOURCE"
#undef main
GPIO_TypeDef gpio_a, gpio_b, gpio_c;
TIM_TypeDef tim2;
static uint16_t inputs = 0xFFFFU;
static uint32_t pending, primask;
static char screen[4][17];
static unsigned templates = 2, enroll_calls, search_calls;
static uint8_t fp_error, cancel_key;
static AS608_StatusCallback progress;
static AS608_CancelCallback cancel;
uint8_t AS608_LastCommand, AS608_LEDStatus;
extern void EXTI0_IRQHandler(void);
extern void EXTI1_IRQHandler(void);
extern void EXTI15_10_IRQHandler(void);
void OLED_Init(void) {}
void OLED_ShowChar(uint8_t row, uint8_t col, char c) {
    assert(row>=1 && row<=4 && col>=1 && col<=16);
    screen[row-1][col-1]=c;
}
void OLED_ShowString(uint8_t row, uint8_t col, const char *s) {
    while (*s) OLED_ShowChar(row,col++,*s++);
}
void OLED_ShowNum(uint8_t row, uint8_t col, uint32_t n, uint8_t len) {
    char out[20]; snprintf(out,sizeof(out),"%0*u",len,(unsigned)n);
    assert(strlen(out)==len); OLED_ShowString(row,col,out);
}
void OLED_ShowSignedNum(uint8_t row,uint8_t col,int32_t n,uint8_t len) {
    OLED_ShowChar(row,col,n<0?'-':'+');
    OLED_ShowNum(row,col+1,(uint32_t)(n<0?-n:n),len);
}
void OLED_ShowHexNum(uint8_t row,uint8_t col,uint32_t n,uint8_t len) {
    char out[20]; snprintf(out,sizeof(out),"%0*X",len,(unsigned)n);
    OLED_ShowString(row,col,out);
}
void GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *cfg) {
    for(unsigned i=0;i<16;i++) if(cfg->GPIO_Pin&(1U<<i)) p->mode[i]=cfg->GPIO_Mode;
}
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *p,uint16_t pin) {
    assert(p==GPIOB); return (inputs & pin) ? Bit_SET:Bit_RESET;
}
void RCC_APB2PeriphClockCmd(uint32_t clock,FunctionalState state) {(void)clock;assert(state==ENABLE);}
void GPIO_EXTILineConfig(uint8_t port,uint8_t pin) {assert(port==1 && (pin==0||pin==1||pin==10));}
void EXTI_Init(EXTI_InitTypeDef *cfg) {assert(cfg->EXTI_LineCmd==ENABLE);}
void EXTI_ClearITPendingBit(uint32_t line) {pending &= ~line;}
ITStatus EXTI_GetITStatus(uint32_t line) {return (pending&line)?SET:RESET;}
void NVIC_Init(NVIC_InitTypeDef *cfg) {assert(cfg->NVIC_IRQChannelCmd==ENABLE);}
uint32_t __get_PRIMASK(void) {return primask;}
void __disable_irq(void) {primask=1;}
void __enable_irq(void) {primask=0;}
void KEY_Init(void) {}
uint8_t KEY_Scan(void) {if(cancel_key){cancel_key=0;return 11;}return KEY_NONE;}
uint8_t KEY_GetLastPosition(void) {return 0;}
void LED_Init(void) {}
void LED_Red_On(void) {}
void LED_Green_On(void) {}
void Buzzer_Init(void) {}
void buzzer1(void) {}
void PWM_Init(void) {}
void PWM_open(void) {}
void AS608_Init(void) {}
void AS608_SetStatusCallback(AS608_StatusCallback cb) {progress=cb;}
void AS608_SetCancelCallback(AS608_CancelCallback cb) {cancel=cb;}
uint8_t AS608_VerifyPassword(void) {return fp_error;}
uint8_t AS608_ReadLibraryInfo(uint16_t *cap,uint16_t *count) {*cap=200;*count=templates;return fp_error;}
uint8_t AS608_FindFreeID(uint16_t *id) {*id=templates;return fp_error;}
uint8_t AS608_Enroll(uint16_t id) {
    enroll_calls++;
    assert(id==templates);
    if(cancel && cancel()) return AS608_ERR_CANCELLED;
    for(uint8_t s=1;s<=5;s++) progress(s);
    templates++; return AS608_OK;
}
uint8_t Finger_Search(uint16_t *id,uint16_t *score) {search_calls++;*id=2;*score=88;return fp_error;}
static void state(unsigned s) {
    uint16_t previous=inputs;
    inputs=(inputs&~3U)|s;
    pending|=(previous^inputs)&3U;
    if(pending&1U) EXTI0_IRQHandler();
    if(pending&2U) EXTI1_IRQHandler();
}
static void poll(unsigned n) {while(n--) Test_PollEncoder();}
static void button(unsigned down) {
    if(down) {inputs&=~(1U<<10);pending|=1U<<10;EXTI15_10_IRQHandler();}
    else inputs|=1U<<10;
}
int main(void) {
    memset(screen,' ',sizeof(screen)); for(unsigned r=0;r<4;r++) screen[r][16]=0;
    Test_EnterEncoder();
    state(1);state(0);state(2);state(3);poll(1);
    assert(test_encoder_position==1);
    state(2);state(0);state(1);state(3);poll(1);
    assert(test_encoder_position==0);
    state(2);state(3);poll(1);assert(test_encoder_position==0);
    state(0);state(3);poll(1);assert(test_encoder_position==0);
    button(1);poll(19);assert(test_encoder_presses==0);
    poll(1);assert(test_encoder_presses==1 && !strncmp(screen[2]+5,"DOWN",4));
    poll(100);assert(test_encoder_presses==1);
    button(0);poll(3);button(1);poll(30);assert(test_encoder_presses==1);
    button(0);poll(20);assert(!strncmp(screen[2]+5,"UP  ",4));
    button(1);poll(20);assert(test_encoder_presses==2);
    primask=1;(void)Encoder_GetDelta();assert(primask==1);primask=0;
    Test_DrawIO();
    Test_KeyEvent(14,12);assert(test_mode==TEST_IO); // C key is NOT physical S14.
    Test_KeyEvent(0,14); // First S14: prepare enrollment, no acquisition or storage.
    assert(test_mode==TEST_FINGER && test_finger_count==2 && test_finger_stage==FP_ENROLL_READY);
    assert(enroll_calls==0 && search_calls==0);
    Test_KeyEvent(1,1);Test_KeyEvent(2,2); // Old numeric shortcuts must not start work.
    assert(enroll_calls==0 && search_calls==0 && test_finger_stage==FP_ENROLL_READY);
    Test_KeyEvent(0,14); // Second S14: execute enrollment, report saved ID.
    assert(templates==3 && test_finger_id==2 && enroll_calls==1 && test_finger_stage==FP_ENROLL_DONE);
    assert(!strncmp(screen[0],"FINGER SAVED",12));
    Test_KeyEvent(0,14); // Third S14: only prepare verification.
    assert(test_finger_stage==FP_MATCH_READY && search_calls==0);
    assert(!strncmp(screen[0],"FP VERIFY READY",15));
    Test_KeyEvent(0,14); // Fourth S14: acquire and compare.
    assert(test_finger_stage==FP_MATCH_DONE && search_calls==1 && !strncmp(screen[0],"MATCH OK",8));
    assert(!strncmp(screen[1]+3,"00002",5) && !strncmp(screen[2]+6,"00088",5));
    Test_KeyEvent(0,14);assert(test_finger_stage==FP_MATCH_READY && search_calls==1);
    fp_error=AS608_ERR_NO_MATCH;Test_KeyEvent(0,14);
    assert(test_finger_stage==FP_MATCH_DONE && !strncmp(screen[2],"NO MATCH",8));
    Test_KeyEvent(0,14);fp_error=AS608_ERR_TIMEOUT;Test_KeyEvent(0,14);
    assert(test_finger_stage==FP_MATCH_READY); // Retry capture after communication failure.
    fp_error=0;Test_KeyEvent(0,14);assert(test_finger_stage==FP_MATCH_DONE);
    Test_KeyEvent(11,13);Test_KeyEvent(0,14);assert(test_finger_count==3);
    fp_error=AS608_ERR_LIB_FULL;Test_KeyEvent(0,14);
    assert(enroll_calls==1 && templates==3 && test_finger_stage==FP_ENROLL_READY);
    fp_error=0;cancel_key=1;Test_KeyEvent(0,14);assert(test_mode==TEST_IO && templates==3);
    fp_error=AS608_ERR_TIMEOUT;Test_KeyEvent(0,14);assert(!test_finger_ready);
    assert(!strncmp(screen[2],"UART TIMEOUT",12));
    fp_error=0;Test_KeyEvent(0,14);assert(test_finger_ready && test_finger_stage==FP_ENROLL_READY);
    puts("PASS: encoder/PB10, physical S14 four-press workflow, prepare-without-capture, repeated match, error/retry/cancel UI");
    return 0;
}
"""

PROTOCOL_TEST = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "AS608.h"
GPIO_TypeDef gpio_a,gpio_b,gpio_c;
SysTick_Type mock_systick;
USART_TypeDef usart2;
uint32_t SystemCoreClock=72000000;
static uint8_t tx[80],rx[100],bitmap[8192];
static unsigned tx_n,rx_n,rx_pos,capacity=300,count=2,stored=9999,gen_n,store_calls;
static unsigned short_reply,bad_checksum,silent,index_error,cancel_at,cancel_n,steps;
static uint8_t commands[100];
static unsigned command_n;
static void reply(uint8_t code,const uint8_t *data,unsigned n) {
    unsigned length=n+3,sum=7+length;
    uint8_t header[]={0xEF,1,255,255,255,255,7,0,0};
    memcpy(rx,header,9);rx[8]=(uint8_t)length;rx[9]=code;sum+=code;
    for(unsigned i=0;i<n;i++){rx[10+i]=data[i];sum+=data[i];}
    rx[10+n]=sum>>8;rx[11+n]=sum;
    if(bad_checksum) rx[11+n]^=1;
    rx_pos=0;rx_n=12+n;
}
static void command(void) {
    unsigned cmd=tx[9];
    commands[command_n++]=cmd;
    if(silent) return;
    if(cmd==0x0F) {
        uint8_t data[16]={0};data[4]=capacity>>8;data[5]=capacity;
        reply(0,data,short_reply?0:16);
    } else if(cmd==0x1D) {
        uint8_t data[2]={count>>8,count};reply(0,data,2);
    } else if(cmd==0x1F) {
        if(index_error) reply(0x1A,0,0);
        else reply(0,bitmap+(unsigned)tx[10]*32U,32);
    } else if(cmd==0x01) {
        gen_n++;reply(gen_n==2?AS608_ERR_NO_FINGER:0,0,0);
    } else if(cmd==0x06) {
        stored=((unsigned)tx[11]<<8)|tx[12];store_calls++;
        assert(stored<capacity && !(bitmap[stored/8]&(1U<<(stored%8))));
        bitmap[stored/8]|=1U<<(stored%8);count++;reply(0,0,0);
    } else if(cmd==0x04) {
        assert(tx[10]==1 && tx[11]==0 && tx[12]==0);
        assert((((unsigned)tx[13]<<8)|tx[14])==capacity);
        uint8_t data[]={0,2,0,88};reply(0,data,4);
    } else reply(0,0,0);
}
void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *cfg) {(void)p;(void)cfg;}
void RCC_APB2PeriphClockCmd(uint32_t c,FunctionalState s){(void)c;(void)s;}
void RCC_APB1PeriphClockCmd(uint32_t c,FunctionalState s){(void)c;(void)s;}
void USART_Init(USART_TypeDef *u,USART_InitTypeDef *cfg){assert(u==USART2 && cfg->USART_BaudRate==57600);}
void USART_Cmd(USART_TypeDef *u,FunctionalState s){(void)u;(void)s;}
FlagStatus USART_GetFlagStatus(USART_TypeDef *u,uint16_t flag) {
    assert(u==USART2);if(flag==USART_FLAG_TXE) return SET;
    if(rx_pos<rx_n) return SET;
    mock_systick.CTRL|=SysTick_CTRL_COUNTFLAG_Msk;return RESET;
}
void USART_SendData(USART_TypeDef *u,uint16_t data) {
    assert(u==USART2 && tx_n<sizeof(tx));tx[tx_n++]=data;
    if(tx_n>=9 && tx_n==9U+(((unsigned)tx[7]<<8)|tx[8])) {
        command();tx_n=0;
    }
}
uint16_t USART_ReceiveData(USART_TypeDef *u){assert(u==USART2 && rx_pos<rx_n);return rx[rx_pos++];}
void Delay_ms(uint32_t ms){(void)ms;}
static uint8_t cancelled(void){return ++cancel_n==cancel_at;}
static void progress(uint8_t step){assert(step==++steps);}
int main(void) {
    uint16_t cap,n,id,score;
    AS608_Init();assert(AS608_VerifyPassword()==0);
    assert(AS608_ReadLibraryInfo(&cap,&n)==0 && cap==300 && n==2);
    bitmap[0]=3;assert(AS608_FindFreeID(&id)==0 && id==2);
    AS608_SetStatusCallback(progress);gen_n=0;command_n=0;
    assert(AS608_Enroll(id)==0 && stored==2 && store_calls==1 && steps==5);
    const uint8_t expected[]={1,2,1,1,2,5,6};
    assert(command_n==sizeof(expected) && !memcmp(commands,expected,sizeof(expected)));
    gen_n=0;assert(Finger_Search(&id,&score)==0 && id==2 && score==88);
    /* The actual driver reinitializes RAM but the emulated module retains Flash. */
    AS608_Init();assert(AS608_ReadLibraryInfo(&cap,&n)==0 && n==3);
    memset(bitmap,255,32);bitmap[32]=3;
    assert(AS608_FindFreeID(&id)==0 && id==258); // second index page
    capacity=10;count=10;assert(AS608_ReadLibraryInfo(&cap,&n)==0);
    assert(AS608_FindFreeID(&id)==AS608_ERR_LIB_FULL && store_calls==1);
    index_error=1;assert(AS608_FindFreeID(&id)==0x1A);index_error=0;
    short_reply=1;assert(AS608_ReadLibraryInfo(&cap,&n)==AS608_ERR_RECV);short_reply=0;
    bad_checksum=1;assert(AS608_VerifyPassword()==AS608_ERR_RECV);bad_checksum=0;
    silent=1;assert(AS608_VerifyPassword()==AS608_ERR_TIMEOUT);silent=0;
    /* Cancel just before Store, ensuring no template is written. */
    capacity=300;count=2;memset(bitmap,0,sizeof(bitmap));
    assert(AS608_ReadLibraryInfo(&cap,&n)==0);
    AS608_SetCancelCallback(cancelled);cancel_at=5;cancel_n=gen_n=steps=0;
    assert(AS608_Enroll(2)==AS608_ERR_CANCELLED && store_calls==1);
    puts("PASS: actual AS608 packets, capacity/count/index, safe enrollment, persistence model, search bounds, malformed ACK, timeout and cancel");
    return 0;
}
"""


def main():
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("Host GCC is required.")
    with tempfile.TemporaryDirectory(prefix="locking-extended-") as directory:
        temp = Path(directory)
        pins = "\n".join(f"#define GPIO_Pin_{i} (1U << {i})" for i in range(16))
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", EXTRA + pins + "\n#endif"))
        for name in ("rcc", "gpio", "usart", "flash"):
            (temp / f"stm32f10x_{name}.h").write_text('#include "stm32f10x.h"\n')
        targets = [
            ("main", MAIN_TEST.replace("MAIN_SOURCE", (ROOT / "basic/user/main.c").as_posix()), "encoder.c"),
            ("as608", PROTOCOL_TEST, "AS608.c"),
        ]
        for name, source, driver in targets:
            harness = temp / f"{name}.c"
            harness.write_text(source)
            exe = temp / f"{name}.exe"
            subprocess.run([
                gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", f"-I{temp}",
                *(f"-I{ROOT / 'basic' / part}" for part in ("user", "hardware", "system")),
                str(harness), str(ROOT / "basic/hardware" / driver), "-o", str(exe),
            ], check=True)
            subprocess.run([str(exe)], check=True, timeout=15)


if __name__ == "__main__":
    main()
