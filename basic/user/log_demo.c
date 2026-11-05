#include "log_demo.h"
#include "log.h"
#include "audit.h"
#include "encoder.h"
#include "pattern_view.h"
#include "key.h"

static uint8_t active, confirm_batch, has_selected;
static uint8_t read_only;
static uint16_t selected;
static uint32_t selected_serial, known_newest;
static uint16_t known_count;

static void PutNumber(char *target, uint32_t value, uint8_t digits)
{
    while (digits) {
        target[--digits] = (char)('0' + value % 10U);
        value /= 10U;
    }
}

static const char *Describe(const Log_Record_t *record)
{
    if (record->method == LOG_METHOD_PASSWORD) {
        if (!record->result) return "NUM FAIL D:0";
        return record->duress ? "NUM PASS D:1" : "NUM PASS D:0";
    }
    if (record->method == LOG_METHOD_DIAL) {
        if (!record->result) return "DIAL FAIL D:0";
        return record->duress ? "DIAL PASS D:1" : "DIAL PASS D:0";
    }
    if (record->method == LOG_METHOD_FINGER) return record->result ? "FING PASS D:0" : "FING FAIL D:0";
    return "TEST PASS D:0";
}

static void ShowRecord(void)
{
    Log_Record_t record, newest;
    uint16_t total = Log_GetCount();
    char header[] = "LOG 000/000";
    char number[] = "ID:0000000000";
    char pending[] = "Pending:0 Err:0";
    known_count = total;
    known_newest = Log_Read(0, &newest) ? newest.serial : 0;
    if (!total) {
        has_selected = 0;
        pending[8] = (char)('0' + Audit_GetPendingCount());
        pending[14] = (char)('0' + Audit_HasWriteError());
        PatternView_Message("AUDIT LOG", "No records", pending, read_only ? "0 Back # Refresh" : "A Test 0Back #Up");
        return;
    }
    if (selected >= total) selected = total - 1U;
    if (!Log_Read(selected, &record)) {
        has_selected = 0;
        PatternView_Message("AUDIT LOG", "Read failed", "# Refresh scan", "0 Back");
        return;
    }
    has_selected = 1;
    selected_serial = record.serial;
    PutNumber(header + 4, selected + 1U, 3);
    PutNumber(header + 8, total, 3);
    PutNumber(number + 3, record.serial, 10);
    PatternView_Message(header, number, Describe(&record), "Turn 0Back #Upd");
}

static void Diagnostics(void)
{
    char count[] = "N:000 Bad:000";
    char state[] = "Pending:0 Err:0";
    PutNumber(count + 2, Log_GetCount(), 3);
    PutNumber(count + 10, Log_GetInvalidCount(), 3);
    state[8] = (char)('0' + Audit_GetPendingCount());
    state[14] = (char)('0' + Audit_HasWriteError());
    PatternView_Message("LOG STATUS", count, state, "# View 0 Back");
}

void LogDemo_Start(void)
{
    read_only = 0;
    active = 1;
    confirm_batch = 0;
    selected = 0;
    has_selected = 0;
    Log_Init();
    Encoder_ClearCount();
    ShowRecord();
}

void LogDemo_StartReadOnly(void) { LogDemo_Start(); read_only = 1; ShowRecord(); }

uint8_t LogDemo_IsActive(void) { return active; }

void LogDemo_Update(uint8_t key)
{
    int8_t direction;
    uint8_t moved = 0;
    uint16_t i;
    Log_Record_t newest, record;
    if (confirm_batch) {
        while (Encoder_PopStep(&direction)) { }
        if (key == 0U) { confirm_batch = 0; ShowRecord(); }
        else if (key == 15U) {
            confirm_batch = 0;
            PatternView_Message("ADDING TEST DATA", "130 TEST records", "", "Please wait...");
            for (i = 0; i < 130U; i++) {
                if (!Log_WriteEvent(LOG_METHOD_TEST, LOG_RESULT_PASS, 0)) break;
            }
            Encoder_ClearCount();
            selected = 0;
            if (i != 130U) PatternView_Message("LOG WRITE FAILED", "Check log status", "# Refresh", "0 Back");
            else ShowRecord();
        }
        return;
    }
    if (key == 0U) { active = 0; Encoder_ClearCount(); return; }
    if (!read_only && key == 13U) {
        confirm_batch = 1;
        Encoder_ClearCount();
        PatternView_Message("ADD 130 TEST?", "Old pages rotate", "# Run 0 Cancel", "Marked TEST");
        return;
    }
    if (!read_only && key == 12U) {
        if (Log_WriteEvent(LOG_METHOD_TEST, LOG_RESULT_PASS, 0)) { selected = 0; ShowRecord(); }
        else PatternView_Message("LOG WRITE FAILED", "# Refresh", "", "0 Back");
        return;
    }
    if (key == 14U) { Diagnostics(); return; }
    if (key == 15U) { Log_Init(); ShowRecord(); return; }
    /* Keep the selected event anchored if the background queue appends a record. */
    if (Log_GetCount() != known_count || (Log_Read(0, &newest) && newest.serial != known_newest)) {
        if (has_selected) {
            for (i = 0; i < Log_GetCount(); i++) {
                if (Log_Read(i, &record) && record.serial == selected_serial) { selected = i; break; }
            }
        }
        moved = 1;
    }
    if (Encoder_TakeOverflow()) { Encoder_ClearCount(); moved = 1; }
    while (Encoder_PopStep(&direction)) {
        if (direction > 0 && selected + 1U < Log_GetCount()) selected++;
        else if (direction < 0 && selected > 0U) selected--;
        moved = 1;
    }
    if (moved) ShowRecord();
}
