"""Administrator storage/editor and full four-function UI host regression."""
import shutil
import tempfile
from pathlib import Path
from admin_test_support import FLASH_MODEL, UI_MODEL, DRIVERS
from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target

STORE_TEST = FLASH_MODEL + r'''
#include "credential_editor.h"
static int admin_equal(const AdminConfig *a,const AdminConfig *b) {
    if(a->count!=b->count || a->pending_id!=b->pending_id)return 0;
    for(unsigned i=0;i<a->count;i++)if(a->ids[i]!=b->ids[i])return 0;
    return 1;
}
static void rotation_faults(unsigned fill) {
    uint16_t snapshot[32768];
    AdminConfig old,next,loaded;
    reset_flash();AdminStore_Clear(&old);old.count=1;old.ids[0]=0;
    for(unsigned i=0;i<fill;i++)assert(AdminStore_Save(&old)==ADMIN_STORE_OK);
    memcpy(snapshot,flash_image,sizeof(snapshot));next=old;next.pending_id=1;
    for(int partial=0;partial<2;partial++)for(int fail=0;fail<30;fail++) {
        memcpy(flash_image,snapshot,sizeof(snapshot));operations=0;fail_at=fail;partial_failure=partial;
        AdminStoreResult result=AdminStore_Save(&next);fail_at=-1;partial_failure=0;
        assert(AdminStore_Load(&loaded)==ADMIN_STORE_OK);
        assert(admin_equal(&loaded,result==ADMIN_STORE_OK?&next:&old));
        assert(AdminStore_Save(&next)==ADMIN_STORE_OK);check_isolation();
    }
}
int main(void) {
    AdminConfig c,loaded,old;
    CredentialConfig base=make_config(0),check;
    CredentialEditor editor;
    InputEvent event;
    reset_flash();AdminStore_Clear(&c);
    assert(AdminStore_Load(&loaded)==ADMIN_STORE_EMPTY && !operations);
    c.count=1;c.ids[0]=0;assert(AdminStore_Save(&c)==ADMIN_STORE_OK);
    assert(AdminStore_Load(&loaded)==ADMIN_STORE_OK && AdminStore_Contains(&loaded,0));
    assert(!AdminStore_Contains(&loaded,1));c.pending_id=1;
    assert(AdminStore_Save(&c)==ADMIN_STORE_OK && !AdminStore_Contains(&c,1));
    c.ids[c.count++]=1;c.pending_id=ADMIN_NO_ID;
    for(unsigned i=0;i<100;i++)assert(AdminStore_Save(&c)==ADMIN_STORE_OK);
    assert(AdminStore_Load(&loaded)==ADMIN_STORE_OK && admin_equal(&c,&loaded) && erases);
    uint16_t snapshot[32768];memcpy(snapshot,flash_image,sizeof(snapshot));old=c;c.pending_id=2;
    for(int i=0;i<30;i++){
        memcpy(flash_image,snapshot,sizeof(snapshot));operations=0;fail_at=i;
        AdminStoreResult result=AdminStore_Save(&c);fail_at=-1;
        assert(AdminStore_Load(&loaded)==ADMIN_STORE_OK);
        assert(admin_equal(&loaded,result==ADMIN_STORE_OK?&c:&old));
        assert(AdminStore_Save(&c)==ADMIN_STORE_OK);check_isolation();
    }
    post_commit_error=1;assert(AdminStore_Save(&c)==ADMIN_STORE_OK);post_commit_error=0;
    c.ids[1]=0;assert(AdminStore_Save(&c)==ADMIN_STORE_INVALID);
    reset_flash();flash_image[index_of(ADMIN_START_ADDRESS)]=0;
    assert(AdminStore_Load(&c)==ADMIN_STORE_INVALID);AdminStore_Clear(&c);
    assert(AdminStore_Save(&c)==ADMIN_STORE_INVALID); /* corrupt roles never open provisioning */
    rotation_faults(1);rotation_faults(32);rotation_faults(64);
    reset_flash();assert(CredentialStore_Save(&base)==CREDENTIAL_STORE_OK);
    assert(CredentialEditor_Begin(&editor,0,INPUT_METHOD_PASSWORD));
    uint8_t pwd[6]={2,2,2,2,2,2};InputEvent_SubmitPassword(&event,pwd);
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_FIRST_ACCEPTED);
    CredentialEditor_Cancel(&editor);assert(CredentialStore_Load(&check)==CREDENTIAL_STORE_OK && equals(&base,&check));
    assert(CredentialEditor_Begin(&editor,0,INPUT_METHOD_PASSWORD));
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_FIRST_ACCEPTED);
    event.password[0]=3;assert(CredentialEditor_Submit(&editor,&event)==EDIT_MISMATCH);
    assert(CredentialStore_Load(&check)==CREDENTIAL_STORE_OK && equals(&base,&check));
    InputEvent_SubmitPassword(&event,base.duress_password);
    assert(CredentialEditor_Begin(&editor,0,INPUT_METHOD_PASSWORD));
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_FIRST_ACCEPTED);
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_CONFLICT);
    InputEvent_SubmitPassword(&event,pwd);
    assert(CredentialEditor_Begin(&editor,0,INPUT_METHOD_PASSWORD));
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_FIRST_ACCEPTED);block_credential=1;
    assert(CredentialEditor_Submit(&editor,&event)==EDIT_SAVE_FAILED);block_credential=0;
    assert(CredentialStore_Load(&check)==CREDENTIAL_STORE_OK && equals(&base,&check));
    check_isolation();puts("PASS: administrator ID0, pending exclusion, 100 journal saves/rotation, write fault sweep, corrupt fail-closed; editor cancel/mismatch/conflict/write failure");
    return 0;
}
'''

UI_TEST = FLASH_MODEL + UI_MODEL + r'''
int main(void) {
    AdminConfig roles;
    CredentialConfig expected=make_config(0),loaded;
    Log_Record_t log;
    assert(equals(&expected,&expected));
    for(unsigned stage=0;stage<3;stage++) {
        init_test();key(12);key(15);
        if(stage>=1)key(15);
        if(stage>=2){present=0;key(15);}
        key(0);assert(!strcmp(screen[0],"LOCKING") && !stores);
        assert(AdminStore_Load(&roles)==ADMIN_STORE_OK && !roles.count);
    }
    init_test();assert(!operations && !strcmp(screen[0],"LOCKING"));
    assert(home_title_x==36 && !strcmp(screen[1],"1 Keypad PIN") &&
           !strcmp(screen[2],"2 Dial PIN") && !strcmp(screen[3],"A Admin"));
    key(1);assert(state==STATE_KEYPAD_INPUT && !strcmp(screen[0],"NUMBER"));key(0);
    key(2);assert(state==STATE_DIAL_INPUT && !strcmp(screen[0],"DIAL"));key(0);
    key(1);digits("111111");assert(!strcmp(screen[2],"NO CFG"));key(0);
    key(2);turn(3,2,4);assert(!strcmp(screen[2],"NO CFG"));key(0);
    assert(!short_beeps && !alarms && !opens && !green_lit && !red_lit && !servo_opened);
    key(4);key(3);assert(!operations && !strcmp(screen[0],"LOCKING")); /* no ungated menu */
    key(12);key(0);assert(!strcmp(screen[0],"LOCKING"));
    enroll_initial_admin();assert(stores==1 && fingers[0]==10 && fingers[1]==20);
    assert(AdminStore_Load(&roles)==ADMIN_STORE_OK && roles.count==1 && !AdminStore_Contains(&roles,0));
    present=10;key(15);assert(!strcmp(screen[1],"Not admin finger"));key(0);
    login();assert(!strcmp(screen[0],"NO PASSWORDS"));int writes=operations;
    key(KEY_NONE);assert(operations==writes);key(15);assert(!strcmp(screen[0],"COMPLETED"));tick();
    assert(CredentialStore_Load(&loaded)==CREDENTIAL_STORE_OK && equals(&loaded,&expected));
    /* New numeric normal: first and second entry; change takes effect immediately. */
    key(1);key(1);assert(!strcmp(screen[2],"1st #OK"));digits("222222");
    assert(!strcmp(screen[2],"2nd #OK"));digits("222222");assert(!strcmp(screen[0],"COMPLETED"));tick();
    expected.password[0]=expected.password[1]=expected.password[2]=expected.password[3]=expected.password[4]=expected.password[5]=2;
    key(2);key(1);digits("333333");digits("333333");tick();memset(expected.duress_password,3,6);
    key(1);key(2);turn(4,2,4);assert(!strcmp(screen[2],"2nd #OK"));turn(4,2,4);tick();expected.dial.steps[0]=4;
    key(2);key(2);turn(2,4,4);turn(2,4,4);tick();expected.duress_dial.steps[1]=4;
    assert(CredentialStore_Load(&loaded)==CREDENTIAL_STORE_OK && equals(&loaded,&expected));
    key(1);key(1);digits("444444");digits("555555");assert(!strcmp(screen[1],"Inputs differ"));key(0);
    key(1);key(1);digits("333333");digits("333333");assert(!strcmp(screen[1],"Normal=duress"));key(0);
    key(1);key(2);turn(6,2,4);key(0);assert(CredentialStore_Load(&loaded)==CREDENTIAL_STORE_OK && equals(&loaded,&expected));
    /* Three stages wait for explicit # and verify removal. */
    present=30;key(3);unsigned commands=fp_commands;key(KEY_NONE);key(1);assert(fp_commands==commands);
    key(15);assert(!strcmp(screen[0],"ENROLL 2/3"));key(15);assert(!strcmp(screen[1],"Lift finger now"));
    key(15);assert(!strcmp(screen[0],"ENROLL 2/3"));present=0;key(15);assert(!strcmp(screen[0],"ENROLL 3/3"));
    present=31;key(15);assert(!strcmp(screen[1],"Different finger"));assert(stores==1);key(15);
    present=30;key(15);present=0;key(15);present=30;key(15);assert(stores==2);tick();
    assert(AdminStore_Load(&roles)==ADMIN_STORE_OK && roles.count==2);
    present=30;key(3);key(15);assert(!strcmp(screen[1],"Finger exists"));key(0);
    /* Read-only admin viewer cannot append TEST or bulk data. */
    key(4);unsigned total=Log_GetCount();writes=operations;key(12);key(13);key(15);
    assert(Log_GetCount()==total && operations==writes);key(0);assert(!strcmp(screen[0],"1Normal 2Duress"));
    key(0);assert(!strcmp(screen[0],"LOCKING"));key(4);assert(!strcmp(screen[0],"LOCKING"));
    assert(!short_beeps && !alarms && !opens); /* Admin actions never unlock or alarm. */
    key(1);digits("222222");assert(!strcmp(screen[2],"PASS"));key(15);assert(Log_Read(0,&log) && log.method==1 && log.result==1 && !log.duress);
    assert(short_beeps==1 && !alarms && green_lit && !red_lit && servo_opened && opens==1);
    now+=10000;key(KEY_NONE);assert(servo_opened && green_lit && short_beeps==1); /* Hold until 0. */
    key(11);assert(InputCnt==5);for(unsigned i=0;i<5;i++)key(11);
    digits("333333");assert(!strcmp(screen[2],"PASS"));assert(Log_Read(0,&log) && log.duress);
    assert(short_beeps==2 && !alarms && green_lit && !red_lit && servo_opened && opens==2);
    key(0);assert(!servo_opened && !green_lit && !red_lit);
    key(2);turn(4,2,4);assert(!strcmp(screen[2],"PASS"));key(11);turn(2,4,4);assert(!strcmp(screen[2],"PASS"));
    assert(Log_Read(0,&log) && log.method==2 && log.duress);
    assert(short_beeps==4 && !alarms && green_lit && !red_lit && servo_opened && opens==4);
    key(15);assert(short_beeps==4 && opens==4);key(0);
    assert(!servo_opened && !green_lit && !red_lit);
    key(1);digits("444444");assert(!strcmp(screen[2],"FAIL"));
    assert(alarms==1 && short_beeps==4 && red_lit && !green_lit && !servo_opened);
    key(15);now+=10000;key(KEY_NONE);assert(alarms==1 && red_lit);key(0);
    assert(!red_lit && !green_lit && !servo_opened);
    key(2);turn(4,2,4);assert(short_beeps==5 && green_lit && servo_opened);
    key(11);turn(1,1,1);assert(!strcmp(screen[2],"NO MATCH"));
    assert(alarms==2 && short_beeps==5 && red_lit && !green_lit && !servo_opened);
    key(15);assert(alarms==2);key(0);
    key(1);key(1);key(15);assert(!strcmp(screen[2],"# Need6"));key(0);
    key(2);turn(3,0,0);assert(!strcmp(screen[2],"Need 3"));key(0);
    key(2);force_overflow=1;turn(3,2,4);assert(shown.invalid);key(0);
    assert(short_beeps==5 && alarms==2 && !servo_opened && !green_lit && !red_lit);
    writes=operations;App_Init();assert(operations==writes);login();key(0);
    key(12);cancel_search=1;key(15);cancel_search=0;assert(!strcmp(screen[0],"LOCKING"));
    key(12);injected_error=AS608_ERR_TIMEOUT;AS608_LastCommand=0x13;
    AS608_LastDiag.tx_bytes=16;AS608_LastDiag.tx_complete=1;
    key(15);assert(!strcmp(screen[0],"NOT COMPLETED"));unsigned nsearch=searches;
    key(13);assert(!strcmp(screen[0],"C13 TX016 TC1") && !strcmp(screen[1],"RX000 E00 D000"));
    key(14);assert(!strcmp(screen[0],"BRR0271 S2000") && !strcmp(screen[1],"CR1:202C I1 P0"));
    unsigned saved=stores;key(10);assert(!strcmp(screen[0],"LINK TEST ONLY"));
    assert(!strcmp(screen[1],"I:FF R000 E00") && !strcmp(screen[2],"P:00 R012 E00") && stores==saved);
    AS608_LastDiag.rx_bytes=0;
    assert(searches==nsearch);injected_error=0;key(0);
    block_audit=1;
    for(unsigned i=0;i<AUDIT_QUEUE_CAPACITY;i++)assert(Audit_Enqueue(LOG_METHOD_FINGER,1,0));
    assert(!Audit_Enqueue(LOG_METHOD_FINGER,1,0));
    key(12);unsigned search_count=searches;key(15);
    assert(!strcmp(screen[1],"Audit busy") && searches==search_count);key(0);
    auth.audit_pending=1;auth.audit.method=INPUT_METHOD_PASSWORD;auth.audit.success=1;auth.audit.duress=0;
    key(1);digits("222222");assert(!strcmp(screen[2],"BUSY"));
    assert(short_beeps==5 && alarms==2 && !servo_opened && !green_lit && !red_lit);key(0);
    block_audit=0;for(unsigned i=0;i<8;i++)key(KEY_NONE);
    assert(!Audit_GetPendingCount() && !Audit_HasWriteError());
    assert(!Audit_Enqueue(LOG_METHOD_FINGER,1,1) && !Audit_Enqueue(99,1,0));
    login();key(1);key(1);digits("444444");block_credential=1;digits("444444");block_credential=0;
    assert(!strcmp(screen[1],"Save failed"));key(0);assert(CredentialStore_Load(&loaded)==CREDENTIAL_STORE_OK && equals(&loaded,&expected));
    assert(short_beeps==5 && alarms==2 && !servo_opened && !green_lit && !red_lit);
    check_isolation();
    /* Power loss between template store and MCU role commit: no automatic grant. */
    init_test();key(12);key(15);key(15);present=0;key(15);present=20;block_admin=1;key(15);
    assert(!strcmp(screen[1],"Role save failed") && fingers[1]==20);block_admin=0;
    App_Init();assert(AdminStore_Load(&roles)==ADMIN_STORE_OK && !roles.count && roles.pending_id==1);
    key(12);key(15);assert(!strcmp(screen[0],"PENDING TEMPLATE"));present=10;key(15);
    assert(!strcmp(screen[1],"No match"));key(15);present=20;key(15);
    assert(!strcmp(screen[0],"COMPLETED") && AdminStore_Load(&roles)==ADMIN_STORE_OK && roles.count==1 && roles.pending_id==ADMIN_NO_ID);
    check_isolation();
    puts("PASS: A/# permission gate, fresh first administrator, four credential edits, retry/cancel/error, three manual fingerprint stages, duplicate/mismatch, read-only logs, logout/reboot, normal/duress audit and interrupted enrollment recovery");
    puts("PASS: keypad/dial normal and duress share short-beep/green/open; rejected codes alarm/red/closed; repeat # is one-shot, 0 resets, incomplete/fault/busy/admin actions have no password feedback");
    return 0;
}
'''

def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required")
    with tempfile.TemporaryDirectory(prefix="host-admin-", dir=ROOT / "basic/build") as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "admin_store_editor", STORE_TEST,
                   ["hardware/admin_store.c", "hardware/credential_editor.c", "hardware/credential_store.c",
                    "hardware/credentials.c", "hardware/dial_sequence.c", "hardware/input_event.c"])
        run_target(temp, "admin_ui", UI_TEST, DRIVERS)

if __name__ == "__main__":
    main()
