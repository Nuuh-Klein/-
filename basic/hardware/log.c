#include "stm32f10x.h"                  // Device header
#include "stddef.h"                     //C语言没带NULL，真烦
#include "config.h"                     // 日志区地址 / 长度等参数
#include "flash.h"                      // FLASH 读写接口
#include "log.h"

//模块内部状态（只保存在 RAM 里，这两个变量掉电即丢失，上电后由 Log_Init() 从 FLASH 里扫描恢复。
static uint16_t log_count;        // 当前有效日志条数（0 ~ LOG_MAX_RECORDS）
static uint16_t log_next_serial;  // 下一条日志要写入的“事件序号”

//内部工具
//读取第 index 条记录在 FLASH 中的某个字段，每条记录定长 6 字节，字段偏移：+0=方式(method)，+2=成败(result)，+4=序号(serial)
static uint16_t Log_ReadMethodAt(uint16_t index){
	return FLASH_ReadHalfWord(LOG_START_ADDRESS + (uint32_t)index * LOG_RECORD_SIZE + 0);
}
static uint16_t Log_ReadResultAt(uint16_t index){
	return FLASH_ReadHalfWord(LOG_START_ADDRESS + (uint32_t)index * LOG_RECORD_SIZE + 2);
}
static uint16_t Log_ReadSerialAt(uint16_t index){
	return FLASH_ReadHalfWord(LOG_START_ADDRESS + (uint32_t)index * LOG_RECORD_SIZE + 4);
}

//擦除整个存储区，两页大小
static void Log_EraseAll(void){
	for(uint16_t i = 0; i < LOG_PAGE_COUNT; i++){
		FLASH_EraseAPage(LOG_START_ADDRESS + (uint32_t)i * LOG_PAGE_SIZE);
	}
}

//上电初始化
//从 FLASH 扫描恢复 log_count 与 log_next_serial
void Log_Init(void){
	/* 因本模块采用“写满整区就擦除重来”的策略，日志永远按槽位 0,1,2,... 连续存放，
	 * 所以只要从 0 号槽向后找到第一条“空记录”（擦除态，其 method 字段 == 0xFFFF），
	 * 这个下标就是有效条数 log_count，同时也是下一条记录的写入位置。 */
	log_count = 0;
	while(log_count < LOG_MAX_RECORDS){
		if(Log_ReadMethodAt(log_count) == 0xFFFF){
			break;   // 遇到空槽，说明后面全都是空的，停止扫描
		}
		log_count++;
	}

//恢复“下一条事件序号”：最后一条日志的序号 +1；日志区全空则从 1 开始
	if(log_count == 0){
		log_next_serial = 1;
	}else{
		log_next_serial = Log_ReadSerialAt(log_count - 1) + 1;//从上一个记录的日志的尾部加1
	}
}

//追加一条开锁事件 ----
//@param method  开锁方式（LOG_METHOD_PASSWORD / PATTERN / FINGER / DURESS）
//@param result  成败（LOG_RESULT_PASS / LOG_RESULT_FAIL）
void Log_Write(uint16_t method, uint16_t result){
  //若区域写满：整区擦除，从头开始写
	if(log_count >= LOG_MAX_RECORDS){
		Log_EraseAll();
		log_count = 0;
	}

	//计算本条记录在 FLASH 中的地址，连续写入 3 个半字 {方式, 成败, 序号}
	uint32_t addr = LOG_START_ADDRESS + (uint32_t)log_count * LOG_RECORD_SIZE;
	FLASH_WriteHalfWord(addr + 0, method);
	FLASH_WriteHalfWord(addr + 2, result);
	FLASH_WriteHalfWord(addr + 4, log_next_serial);

	log_count++;         // 有效条数 +1
	log_next_serial++;   // 事件序号 +1（uint16 自然回绕，回绕后仅影响显示，不影响定位）
}

//返回当前有效日志条数（供“查看日志”菜单判断可翻阅范围）
uint16_t Log_GetCount(void){
	return log_count;
}

//读第recent_index条的记录
//@param recent_index  0=最新一条，log_count-1=最旧一条（配合编码器翻页使用）
//@param record        输出参数：读到的记录内容
//@return 1=读取成功，0=参数非法（越界或 record 为空指针）
uint8_t Log_Read(uint16_t recent_index, Log_Record_t *record){
	if(record == NULL || recent_index >= log_count){
		return 0;
	}

	/* 日志连续存放：最新一条在第 (log_count-1) 槽，往前退 recent_index 个槽即可 */
	uint16_t slot = (log_count - 1) - recent_index;

	record->method = Log_ReadMethodAt(slot);
	record->result = Log_ReadResultAt(slot);
	record->serial = Log_ReadSerialAt(slot);
	return 1;
}
