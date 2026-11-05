"""Shared flash model for administrator journal/editor/UI regression."""
from test_auth import BASE

FLASH_MODEL = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "admin_store.h"
#include "credential_store.h"
#include "flash.h"
static uint16_t flash_image[32768];
static int operations, fail_at = -1, erases, block_admin, block_credential, block_audit, post_commit_error, partial_failure;
static unsigned index_of(uint32_t a) {
    assert(a >= 0x08000000UL && a < 0x08010000UL && !(a & 1U));
    return (a - 0x08000000UL) / 2U;
}
uint16_t FLASH_ReadHalfWord(uint32_t a) { return flash_image[index_of(a)]; }
static int denied(uint32_t a) {
    int n = operations++;
    return n == fail_at || (a < 0x0800DC00UL ? block_admin : a < 0x0800E400UL ? block_audit : block_credential);
}
FLASH_Status FLASH_WriteHalfWord(uint32_t a, uint16_t value) {
    assert(a >= 0x0800D400UL && a < 0x0800EC00UL);
    if (denied(a)) {
        if (partial_failure) flash_image[index_of(a)] &= (uint16_t)(value | 0x00FFU);
        return FLASH_ERROR_PG;
    }
    unsigned i = index_of(a); assert((flash_image[i] & value) == value); flash_image[i] &= value;
    if (post_commit_error && a < 0x0800DC00UL && (a & 63U) == 0) return FLASH_ERROR_PG;
    return FLASH_COMPLETE;
}
FLASH_Status FLASH_EraseAPage(uint32_t a) {
    assert(a >= 0x0800D400UL && a < 0x0800EC00UL && !(a & 1023U));
    if (denied(a)) {
        if (partial_failure) memset(&flash_image[index_of(a)], 255, 512);
        return FLASH_ERROR_PG;
    }
    memset(&flash_image[index_of(a)], 255, 1024); erases++; return FLASH_COMPLETE;
}
static void reset_flash(void) {
    memset(flash_image, 255, sizeof(flash_image)); operations = erases = 0;
    fail_at = -1; block_admin = block_credential = block_audit = post_commit_error = partial_failure = 0;
    flash_image[index_of(0x0800D000UL)] = 0x2468;
    for (uint32_t a = 0x0800EC00UL; a < 0x08010000UL; a += 1024) flash_image[index_of(a)] = 0x1357;
}
static void check_isolation(void) {
    assert(flash_image[index_of(0x0800D000UL)] == 0x2468);
    for (uint32_t a = 0x0800EC00UL; a < 0x08010000UL; a += 1024) assert(flash_image[index_of(a)] == 0x1357);
}
''' + BASE[BASE.index("static CredentialConfig make_config"):]

DRIVERS = ["hardware/admin_store.c", "hardware/admin_fingerprint.c", "hardware/credential_editor.c",
           "hardware/credential_store.c", "hardware/credentials.c", "hardware/auth.c",
           "hardware/dial_sequence.c", "hardware/input_event.c", "hardware/password_input.c",
           "hardware/audit.c", "hardware/log.c", "user/log_demo.c"]

UI_MODEL = r'''
#define LOCKING_HOST_TEST
#include "main.c"
#include "admin_fingerprint.h"
#include "audit.h"
#include "log.h"
#include "key.h"
static char screen[4][40];
static uint32_t now;
static uint16_t fingers[64], present, buffer1, buffer2;
static unsigned fp_commands, stores, searches, injected_error, cancel_search, force_overflow;
static unsigned short_beeps, alarms, green_lit, red_lit, servo_opened, opens, closes;
void buzzer1(void) { short_beeps++; }
void buzzer2(void) { alarms++; }
void LED_Green_On(void) { green_lit=1; }
void LED_Green_Off(void) { green_lit=0; }
void LED_Red_On(void) { red_lit=1; }
void LED_Red_Off(void) { red_lit=0; }
void LED_Alloff(void) { green_lit=red_lit=0; }
void PWM_open(void) { servo_opened=1;opens++; }
void PWM_close(void) { servo_opened=0;closes++; }
static AS608_CancelCallback cancel_callback;
static AS608_StatusCallback status_callback;
static int8_t queue[128];
static unsigned read_pos, write_pos;
uint8_t AS608_LastCommand;
AS608_UARTDiagnostics AS608_LastDiag;
void AS608_GetHardwareState(AS608_HardwareState *s) {
    s->brr=0x271;s->cr1=0x202C;s->cr2=0x2000;s->gpio_modes=0x8B;
    s->remapped=0;s->irq_enabled=1;s->irq_masked=0;s->priority_group=5;s->irq_priority=0x40;
}
void PatternView_Message(const char *a,const char *b,const char *c,const char *d) {
    memset(screen,0,sizeof(screen));strcpy(screen[0],a);strcpy(screen[1],b);strcpy(screen[2],c);strcpy(screen[3],d);
}
static uint8_t home_title_x;
void OLED_FrameClear(void) { memset(screen,0,sizeof(screen)); }
void OLED_DrawText(uint8_t x,uint8_t y,const char *s) {
    assert(y % 16U == 0 && y < 64);
    strcpy(screen[y/16U],s);
    if(!strcmp(s,"LOCKING"))home_title_x=x;
}
void OLED_FrameShow(void) {}
void PasswordView_Input(uint8_t n,const char *hint,const char *exit) {
    strcpy(screen[0],"NUMBER");memset(screen[1],'*',n);screen[1][n]=0;strcpy(screen[2],hint);strcpy(screen[3],exit);
}
static DialSequence shown;
void DialView_Show(const DialSequence *s,const char *hint) { shown=*s;strcpy(screen[0],"DIAL");strcpy(screen[2],hint); }
void OLED_Clear(void) {}
void OLED_ShowString(uint8_t row,uint8_t col,const char *s) { (void)col;strcpy(screen[row-1],s); }
void OLED_ShowChar(uint8_t row,uint8_t col,char c) { (void)row;(void)col;(void)c; }
uint32_t Timebase_Millis(void) { return now; }
uint8_t KEY_Scan(void) { return cancel_search ? 0 : KEY_NONE; }
void Encoder_ClearCount(void) { read_pos=write_pos=0; }
uint8_t Encoder_TakeOverflow(void) { uint8_t x=force_overflow;force_overflow=0;return x; }
uint8_t Encoder_PopStep(int8_t *d) { if(read_pos==write_pos)return 0;*d=queue[read_pos++];return 1; }
void AS608_SetCancelCallback(AS608_CancelCallback cb) { cancel_callback=cb; }
void AS608_SetStatusCallback(AS608_StatusCallback cb) { status_callback=cb; }
uint8_t AS608_VerifyPassword(void) { fp_commands++;return injected_error; }
uint8_t AS608_VerifyPasswordPolled(void) { fp_commands++;AS608_LastDiag.rx_bytes=12;return 0; }
uint8_t AS608_ReadLibraryInfo(uint16_t *capacity,uint16_t *count) {
    *capacity=64;*count=0;for(unsigned i=0;i<64;i++)if(fingers[i])(*count)++;return injected_error;
}
uint8_t AS608_IDOccupied(uint16_t id,uint8_t *occupied) {
    assert(id<64);*occupied=!!fingers[id];return injected_error;
}
uint8_t AS608_FindFreeID(uint16_t *id) {
    for(unsigned i=0;i<64;i++)if(!fingers[i]){*id=i;return injected_error;}
    return AS608_ERR_LIB_FULL;
}
uint8_t AS608_JM101Search(uint16_t *id,uint16_t *score) {
    searches++;*id=*score=0;
    if(cancel_callback && cancel_callback())return AS608_ERR_CANCELLED;
    if(injected_error)return injected_error;
    for(unsigned i=0;i<64;i++)if(fingers[i] && fingers[i]==present){*id=i;*score=88;return 0;}
    return AS608_ERR_NO_MATCH;
}
uint8_t AS608_CaptureOnce(uint8_t buffer) {
    fp_commands++;if(injected_error)return injected_error;if(!present)return AS608_ERR_NO_FINGER;
    if(buffer==1)buffer1=present;else buffer2=present;return 0;
}
uint8_t AS608_SearchBuffer(uint8_t buffer,uint16_t *id,uint16_t *score) {
    assert(buffer==1);*id=*score=0;
    for(unsigned i=0;i<64;i++)if(fingers[i] && fingers[i]==buffer1){*id=i;*score=88;return 0;}
    return AS608_ERR_NO_MATCH;
}
uint8_t AS608_CheckRemoved(void) { return injected_error ? injected_error : present ? AS608_ERR_FINGER_PRESENT : 0; }
uint8_t AS608_MergeBuffers(void) { return buffer1==buffer2 ? 0 : AS608_ERR_MERGE; }
uint8_t AS608_StoreBuffer(uint16_t id) {
    assert(id<64);if(fingers[id])return AS608_ERR_ID_OCCUPIED;
    fingers[id]=buffer1;stores++;return 0;
}
static void key(uint8_t k) { now++;App_Update(k); }
static void tick(void) { now+=1600;key(KEY_NONE); }
static void digits(const char *s) { while(*s)key((uint8_t)(*s++-'0'));key(15); }
static void turn(unsigned r,unsigned l,unsigned last) {
    while(r--)queue[write_pos++]=1;
    while(l--)queue[write_pos++]=-1;
    while(last--)queue[write_pos++]=1;
    key(15);
}
static void init_test(void) {
    reset_flash();memset(fingers,0,sizeof(fingers));fingers[0]=10;present=20;
    now=fp_commands=stores=searches=injected_error=cancel_search=force_overflow=0;
    short_beeps=alarms=green_lit=red_lit=servo_opened=opens=closes=0;
    App_Init();
}
static void enroll_initial_admin(void) {
    key(12);assert(!strcmp(screen[0],"FIRST ADMIN"));key(15);
    assert(!strcmp(screen[0],"ENROLL 1/3"));present=20;key(15);
    assert(!strcmp(screen[0],"ENROLL 2/3"));present=0;key(15);
    assert(!strcmp(screen[0],"ENROLL 3/3"));present=20;key(15);
    assert(!strcmp(screen[0],"COMPLETED"));tick();assert(!strcmp(screen[0],"ADMIN VERIFY"));
}
static void login(void) {
    key(12);assert(!strcmp(screen[0],"ADMIN VERIFY"));
    unsigned n=searches;present=20;key(15);assert(searches==n+1);
    assert(!strcmp(screen[0],"1Normal 2Duress") || !strcmp(screen[0],"NO PASSWORDS"));
}
'''
