"""Host regression tests of the retained AS608/JM101 driver and USART2 IRQ."""
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
void NVIC_DisableIRQ(unsigned);
void NVIC_ClearPendingIRQ(unsigned);
#define USART2_IRQn 38U
typedef struct {uint32_t CTRL, LOAD, VAL;} SysTick_Type;
extern SysTick_Type mock_systick;
#define SysTick (&mock_systick)
#define SysTick_CTRL_CLKSOURCE_Msk 4U
#define SysTick_CTRL_ENABLE_Msk 1U
#define SysTick_CTRL_COUNTFLAG_Msk (1U<<16)
extern uint32_t SystemCoreClock;
typedef struct {uint32_t SR, BRR;} USART_TypeDef;
extern USART_TypeDef usart2;
#define USART2 (&usart2)
#define USART_FLAG_TXE 128U
#define USART_FLAG_RXNE 32U
#define USART_FLAG_TC 64U
#define USART_SR_RXNE 32U
#define USART_IT_RXNE 32U
#define USART_IT_ERR 1U
#define USART_SR_PE 1U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define USART_SR_ORE 8U
#define GPIO_Remap_USART2 8U
#define RCC_APB1Periph_USART2 32U
#define USART_WordLength_8b 8U
#define USART_StopBits_1 1U
#define USART_StopBits_2 2U
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
void USART_DeInit(USART_TypeDef *);
void USART_Cmd(USART_TypeDef *, FunctionalState);
void USART_ITConfig(USART_TypeDef *, uint16_t, FunctionalState);
typedef struct {uint32_t PCLK1_Frequency;} RCC_ClocksTypeDef;
void RCC_GetClocksFreq(RCC_ClocksTypeDef *);
void SystemCoreClockUpdate(void);
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
static unsigned serial_baud,serial_stop,required_baud,required_stop,required_address;
static unsigned tx_block,tc_block,inject_error,password_error;
static unsigned auto_mode,auto_cancel_calls;
static unsigned irq_mask,irq_enabled,uart_irq,dispatching;
void USART2_IRQHandler(void);
static void dispatch_rx(void) {
    if(irq_mask || !irq_enabled || !uart_irq || dispatching) return;
    dispatching=1;
    while(rx_pos<rx_n) {
        usart2.SR=USART_SR_RXNE;
        if(inject_error){usart2.SR|=inject_error;inject_error=0;}
        USART2_IRQHandler();
    }
    if(usart2.SR&15U) USART2_IRQHandler();
    dispatching=0;
}
uint32_t __get_PRIMASK(void){return irq_mask;}
void __disable_irq(void){irq_mask=1;}
void __enable_irq(void){irq_mask=0;dispatch_rx();mock_systick.CTRL|=SysTick_CTRL_COUNTFLAG_Msk;}
void NVIC_DisableIRQ(unsigned irq){assert(irq==USART2_IRQn);irq_enabled=0;}
void NVIC_ClearPendingIRQ(unsigned irq){assert(irq==USART2_IRQn);}
void NVIC_Init(NVIC_InitTypeDef *cfg){
    assert(cfg->NVIC_IRQChannel==USART2_IRQn && cfg->NVIC_IRQChannelCmd==ENABLE);
    irq_enabled=1;dispatch_rx();
}
void USART_ITConfig(USART_TypeDef *u,uint16_t it,FunctionalState enable){
    assert(u==USART2 && (it==USART_IT_RXNE || it==USART_IT_ERR));
    if(enable)uart_irq|=it;else uart_irq&=~it;
}
static uint8_t commands[100];
static unsigned command_n;
static void reply(uint8_t code,const uint8_t *data,unsigned n) {
    unsigned length=n+3,sum=7+length;
    uint8_t header[]={0xEF,1,255,255,255,255,7,0,0};
    memcpy(rx,header,9);memcpy(rx+2,tx+2,4);rx[8]=(uint8_t)length;rx[9]=code;sum+=code;
    for(unsigned i=0;i<n;i++){rx[10+i]=data[i];sum+=data[i];}
    rx[10+n]=sum>>8;rx[11+n]=sum;
    if(bad_checksum) rx[11+n]^=1;
    rx_pos=0;rx_n=12+n;dispatch_rx(); // 整包在主程序开始ReceiveAck之前就到达。
}
static void command(void) {
    unsigned cmd=tx[9];
    unsigned checksum=0;
    for(unsigned i=6;i<tx_n-2;i++) checksum+=tx[i];
    assert((((unsigned)tx[tx_n-2]<<8)|tx[tx_n-1])==(checksum&0xFFFFU));
    commands[command_n++]=cmd;
    if(silent) return;
    if(required_baud && (serial_baud!=required_baud || serial_stop!=required_stop ||
        (((uint32_t)tx[2]<<24)|((uint32_t)tx[3]<<16)|((uint32_t)tx[4]<<8)|tx[5])!=required_address)) return;
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
    } else if(cmd==0x31) {
        assert(tx_n==17 && tx[8]==8 && tx[12]==2 && tx[13]==0 && tx[14]==0x11);
        uint16_t id=((unsigned)tx[10]<<8)|tx[11];
        uint8_t data[2]={0,0};
        if(auto_mode==1 || (bitmap[id/8]&(1U<<(id%8)))) {reply(0x22,data,2);return;}
        if(auto_mode==2) {reply(0x27,data,2);return;}
        if(auto_mode==7) {reply(0,0,0);return;}
        if(auto_mode==6){data[0]=1;data[1]=1;reply(0,data,2);return;}
        reply(0,data,2);
        if(auto_mode==4)return;
        const uint8_t stages[]={1,2,3,1,2,4,5,6};
        const uint8_t captures[]={1,1,1,2,2,0xF0,0xF1,0xF2};
        for(unsigned i=0;i<8;i++) {
            data[0]=stages[i];data[1]=captures[i];
            if(auto_mode==5 && stages[i]==1) continue;
            if(auto_mode==5 && i==1)reply(AS608_ERR_FEATURE,data,2);
            if(auto_mode==3 && stages[i]==4){reply(AS608_ERR_MERGE,data,2);return;}
            if(stages[i]==6) {bitmap[id/8]|=1U<<(id%8);stored=id;store_calls++;count++;}
            reply(0,data,2);
        }
    } else if(cmd==0x32) {
        assert(tx_n==17 && tx[8]==8 && tx[10]==3 && tx[11]==255 && tx[12]==255 && tx[13]==0 && tx[14]==1);
        uint8_t data[5]={0,0,2,0,88};
        if(auto_mode==9){reply(0x24,data,5);return;}
        reply(0,data,5);if(auto_mode==4)return;
        data[0]=1;reply(0,data,5);data[0]=5;
        reply(auto_mode==8?AS608_ERR_NO_MATCH:0,data,5);
    } else if(cmd==0x30) {
        auto_cancel_calls++;reply(0,0,0);
    } else if(cmd==0x13) {
        assert(tx_n==16 && tx[7]==0 && tx[8]==7);
        for(unsigned i=10;i<14;i++) assert(tx[i]==0);
        assert(tx[14]==0 && tx[15]==0x1B);
        reply(password_error?0x13:0,0,0);
    } else reply(0,0,0);
}
void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *cfg) {
    assert(p==GPIOA);
    if(cfg->GPIO_Pin==GPIO_Pin_3) assert(cfg->GPIO_Mode==GPIO_Mode_IPU);
}
void GPIO_PinRemapConfig(uint32_t map,FunctionalState state) {
    assert((map==GPIO_Remap_USART2 && state==DISABLE) ||
           (map==GPIO_Remap_SWJ_JTAGDisable && state==ENABLE));
}
void RCC_APB2PeriphClockCmd(uint32_t c,FunctionalState s){(void)c;(void)s;}
void RCC_APB1PeriphClockCmd(uint32_t c,FunctionalState s){(void)c;(void)s;}
void USART_Init(USART_TypeDef *u,USART_InitTypeDef *cfg){
    assert(u==USART2);serial_baud=cfg->USART_BaudRate;serial_stop=cfg->USART_StopBits;
    u->BRR=36000000U/serial_baud;
}
void USART_DeInit(USART_TypeDef *u){assert(u==USART2);u->SR=0;tx_n=rx_n=rx_pos=uart_irq=0;}
void USART_Cmd(USART_TypeDef *u,FunctionalState s){(void)u;(void)s;}
FlagStatus USART_GetFlagStatus(USART_TypeDef *u,uint16_t flag) {
    assert(u==USART2);if(flag==USART_FLAG_TXE) return tx_block?RESET:SET;
    if(flag==USART_FLAG_TC) return tc_block?RESET:SET;
    if(rx_pos<rx_n) {if(inject_error){u->SR|=inject_error;inject_error=0;}return SET;}
    mock_systick.CTRL|=SysTick_CTRL_COUNTFLAG_Msk;return RESET;
}
void USART_SendData(USART_TypeDef *u,uint16_t data) {
    assert(u==USART2 && tx_n<sizeof(tx));tx[tx_n++]=data;
    if(tx_n>=9 && tx_n==9U+(((unsigned)tx[7]<<8)|tx[8])) {
        command();tx_n=0;
    }
}
uint16_t USART_ReceiveData(USART_TypeDef *u){assert(u==USART2);u->SR=0;return rx_pos<rx_n?rx[rx_pos++]:0;}
void Delay_ms(uint32_t ms){(void)ms;}
static uint8_t cancelled(void){return ++cancel_n==cancel_at;}
static void progress(uint8_t step){assert(step==++steps);}
int main(void) {
    uint16_t cap,n,id,score;
    AS608_Init();assert(command_n==0 && serial_stop==USART_StopBits_2);
    assert(AS608_VerifyPassword()==0);
    assert(AS608_LastDiag.tx_bytes==16 && AS608_LastDiag.tx_complete && AS608_LastDiag.rx_bytes==12 && AS608_LastDiag.ack_valid);
    assert(AS608_LastDiag.first_rx[0]==0xEF && AS608_LastDiag.first_rx[1]==1);
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
    assert(AS608_LastDiag.rx_bytes==0 && !AS608_LastDiag.ack_valid && AS608_LastDiag.tx_complete);
    password_error=1;assert(AS608_VerifyPassword()==0x13 && AS608_LastDiag.ack_valid);password_error=0;
    inject_error=USART_SR_FE;assert(AS608_VerifyPassword()==AS608_ERR_TIMEOUT);
    assert(AS608_LastDiag.uart_errors==USART_SR_FE && AS608_LastDiag.rx_bytes==12);
    assert(AS608_VerifyPassword()==0 && AS608_LastDiag.uart_errors==0);
    usart2.SR=USART_SR_ORE;assert(AS608_VerifyPassword()==0 && usart2.SR==0);
    tx_block=1;assert(AS608_VerifyPassword()==AS608_ERR_TIMEOUT && AS608_LastDiag.tx_bytes==0);tx_block=0;
    tc_block=1;assert(AS608_VerifyPassword()==AS608_ERR_TIMEOUT && !AS608_LastDiag.tx_complete);tc_block=0;
    /* Full ACK arrives during SendCmd; delay parsing and verify it was retained. */
    uint8_t password[4]={0};
    assert(AS608_SendCmd(0x13,password,4)==AS608_OK && rx_pos==rx_n && rx_n==12);
    assert(AS608_ReceiveAck(300)==AS608_OK && AS608_LastDiag.rx_bytes==12 && !AS608_LastDiag.uart_errors);
    /* Exercise queue wrap without resetting head/tail between received frames. */
    for(unsigned i=0;i<12;i++) {reply(0,0,0);assert(AS608_ReceiveAck(300)==AS608_OK);}
    assert(AS608_LastDiag.rx_dropped==0);
    /* Overflow is reported separately; a truncated transaction must not be valid. */
    assert(AS608_SendCmd(0x13,password,4)==AS608_OK);
    for(unsigned batch=0;batch<3;batch++) {
        memset(rx,0x55,64);rx_pos=0;rx_n=64;dispatch_rx();
    }
    assert(AS608_ReceiveAck(300)==AS608_ERR_RECV && !AS608_LastDiag.ack_valid);
    assert(AS608_LastDiag.rx_bytes==204 && AS608_LastDiag.rx_dropped==77 && !AS608_LastDiag.uart_errors);
    assert(AS608_VerifyPassword()==AS608_OK && AS608_LastDiag.rx_dropped==0);
    /* Cancel just before Store, ensuring no template is written. */
    capacity=300;count=2;memset(bitmap,0,sizeof(bitmap));
    assert(AS608_ReadLibraryInfo(&cap,&n)==0);
    AS608_SetCancelCallback(cancelled);cancel_at=5;cancel_n=gen_n=steps=0;
    assert(AS608_Enroll(2)==AS608_ERR_CANCELLED && store_calls==1);
    puts("PASS: IRQ buffers full ACK before parser, queue wrap/overflow recovery, VfyPwd frame, 8N2, UART errors, enrollment/search and cancellation");
    return 0;
}
"""

JM101_TEST = PROTOCOL_TEST.split("int main(void) {")[0] + r"""
int main(void){
    uint16_t id,score,n;
    AS608_Init();AS608_SetStatusCallback(progress);
    assert(AS608_ReadTemplateCount(&n)==AS608_OK && n==2);
    steps=0;assert(AS608_JM101Enroll(2)==AS608_OK && stored==2 && count==3 && steps==5);
    assert(AS608_JM101Search(&id,&score)==AS608_OK && id==2 && score==88);
    assert(AS608_JM101Enroll(2)==AS608_ERR_ID_OCCUPIED && store_calls==1);
    auto_mode=2;assert(AS608_JM101Enroll(3)==AS608_ERR_DUPLICATE && store_calls==1);
    auto_mode=3;steps=0;assert(AS608_JM101Enroll(3)==AS608_ERR_MERGE && store_calls==1);
    auto_mode=5;steps=0;assert(AS608_JM101Enroll(3)==AS608_OK && store_calls==2 && steps==5);
    auto_mode=6;assert(AS608_JM101Enroll(4)==AS608_ERR_RECV && auto_cancel_calls==1 && store_calls==2);
    auto_mode=7;assert(AS608_JM101Enroll(4)==AS608_ERR_RECV && auto_cancel_calls==2);
    auto_mode=8;assert(AS608_JM101Search(&id,&score)==AS608_ERR_NO_MATCH && id==0 && score==0);
    auto_mode=9;assert(AS608_JM101Search(&id,&score)==0x24 && id==0 && score==0);
    auto_mode=4;steps=0;cancel_at=3;cancel_n=0;AS608_SetCancelCallback(cancelled);
    assert(AS608_JM101Enroll(4)==AS608_ERR_CANCELLED && auto_cancel_calls==3 && store_calls==2);
    assert(AS608_LastCommand==0x31);
    AS608_SetCancelCallback(0);steps=0;
    assert(AS608_JM101Enroll(4)==AS608_ERR_WAIT_TIMEOUT && auto_cancel_calls==4 && store_calls==2);
    auto_mode=0;AS608_Init();assert(AS608_ReadTemplateCount(&n)==AS608_OK && n==4);
    assert(AS608_JM101Search(&id,&score)==AS608_OK && store_calls==2);
    puts("PASS: JM101 0x31/0x32 frames, multi-ACK stages, no overwrite, duplicate/mismatch, timeout/cancel, persistence model");
    return 0;
}
"""

def main():
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-extended-", dir=build_root) as directory:
        temp = Path(directory)
        pins = "\n".join(f"#define GPIO_Pin_{i} (1U << {i})" for i in range(16))
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", EXTRA + pins + "\n#endif"))
        for name in ("rcc", "gpio", "usart", "flash"):
            (temp / f"stm32f10x_{name}.h").write_text('#include "stm32f10x.h"\n')
        targets = [("as608", PROTOCOL_TEST, "AS608.c"),
                   ("jm101", JM101_TEST, "AS608.c")]
        for name, source, driver in targets:
            harness = temp / f"{name}.c"
            harness.write_text(source)
            exe = temp / f"{name}.exe"
            subprocess.run([
                gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", f"-I{temp}",
                *(f"-I{ROOT / 'basic' / part}" for part in ("user", "hardware", "system")),
                str(harness), *([str(ROOT / "basic/hardware" / driver)] if driver else []), "-o", str(exe),
            ], check=True)
            subprocess.run([str(exe)], check=True, timeout=15)


if __name__ == "__main__":
    main()
