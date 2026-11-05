#include "audit.h"
#include "log.h"

typedef struct { uint16_t method; uint8_t success, duress; } QueuedAudit;
static QueuedAudit queue[AUDIT_QUEUE_CAPACITY];
static uint8_t head, tail, pending, write_error;

uint8_t Audit_Init(void)
{
    head = tail = pending = write_error = 0;
    return Log_Init();
}

uint8_t Audit_Capture(AuthContext *context, AuthAuditEvent *diagnostic_copy)
{
    AuthAuditEvent event;
    if (context == 0 || pending == AUDIT_QUEUE_CAPACITY || !Auth_TakeAudit(context, &event)) return 0;
    queue[tail].method = (uint16_t)event.method;
    queue[tail].success = event.success;
    queue[tail].duress = event.duress;
    tail = (uint8_t)((tail + 1U) % AUDIT_QUEUE_CAPACITY);
    pending++;
    if (diagnostic_copy != 0) *diagnostic_copy = event;
    return 1;
}

uint8_t Audit_Process(void)
{
    const QueuedAudit *event;
    if (!pending) return 0;
    event = &queue[head];
    if (!Log_WriteEvent((uint16_t)event->method, event->success, event->duress)) {
        write_error = 1;
        return 0;
    }
    queue[head].method = INPUT_METHOD_NONE;
    queue[head].success = queue[head].duress = 0;
    head = (uint8_t)((head + 1U) % AUDIT_QUEUE_CAPACITY);
    pending--;
    write_error = 0;
    return 1;
}

uint8_t Audit_Enqueue(uint16_t method, uint8_t success, uint8_t duress)
{
    if (pending == AUDIT_QUEUE_CAPACITY || method < LOG_METHOD_PASSWORD || method > LOG_METHOD_FINGER ||
        success > 1U || duress > 1U || (duress && (!success || method == LOG_METHOD_FINGER))) return 0;
    queue[tail].method = method; queue[tail].success = success; queue[tail].duress = duress;
    tail = (uint8_t)((tail + 1U) % AUDIT_QUEUE_CAPACITY); pending++;
    return 1;
}

uint8_t Audit_GetPendingCount(void) { return pending; }
uint8_t Audit_HasWriteError(void) { return write_error; }
