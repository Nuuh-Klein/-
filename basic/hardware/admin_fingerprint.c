#include "admin_fingerprint.h"

uint8_t AdminFinger_Prepare(void)
{
    uint16_t capacity, count;
    uint8_t ack = AS608_VerifyPassword();
    return ack == AS608_OK ? AS608_ReadLibraryInfo(&capacity, &count) : ack;
}

uint8_t AdminFinger_Begin(AdminConfig *config, uint8_t *recover)
{
    AdminConfig next;
    uint8_t ack, occupied;
    uint16_t id;
    *recover = 0;
    if (config->count >= ADMIN_MAX_COUNT) return AS608_ERR_LIB_FULL;
    ack = AdminFinger_Prepare();
    if (ack != AS608_OK) return ack;
    if (config->pending_id != ADMIN_NO_ID) {
        ack = AS608_IDOccupied(config->pending_id, &occupied);
        if (ack != AS608_OK) return ack;
        if (occupied) { *recover = 1; return AS608_OK; }
    }
    ack = AS608_FindFreeID(&id);
    if (ack != AS608_OK) return ack;
    /* Never reuse an ID with an existing administrator role if module data changed. */
    if (AdminStore_Contains(config, id)) return AS608_ERR_ID_OCCUPIED;
    next = *config; next.pending_id = id;
    if (AdminStore_Save(&next) != ADMIN_STORE_OK) return AS608_ERR_SAVE_CONFIG;
    *config = next;
    return AS608_OK;
}

uint8_t AdminFinger_First(void)
{
    uint16_t id, score;
    uint8_t ack = AS608_CaptureOnce(1);
    if (ack != AS608_OK) return ack;
    ack = AS608_SearchBuffer(1, &id, &score);
    return ack == AS608_ERR_NO_MATCH ? AS608_OK : ack == AS608_OK ? AS608_ERR_DUPLICATE : ack;
}

uint8_t AdminFinger_Second(void)
{
    uint8_t ack = AS608_CaptureOnce(2);
    return ack == AS608_OK ? AS608_MergeBuffers() : ack;
}

uint8_t AdminFinger_Commit(AdminConfig *config)
{
    AdminConfig next;
    if (config->count >= ADMIN_MAX_COUNT || config->pending_id == ADMIN_NO_ID) return AS608_ERR_RECV;
    next = *config;
    next.ids[next.count++] = next.pending_id; next.pending_id = ADMIN_NO_ID;
    if (AdminStore_Save(&next) != ADMIN_STORE_OK) return AS608_ERR_SAVE_CONFIG;
    *config = next;
    return AS608_OK;
}

uint8_t AdminFinger_Recover(AdminConfig *config)
{
    uint16_t id, score;
    uint8_t ack = AdminFinger_Prepare();
    if (ack == AS608_OK) ack = AS608_JM101Search(&id, &score);
    if (ack != AS608_OK) return ack;
    if (id != config->pending_id) return AS608_ERR_NO_MATCH;
    return AdminFinger_Commit(config);
}
