#ifndef FLASH_H
#define FLASH_H

#include "main.h" 
#include "bq76940.h"

#define FLASH_STORAGE_ADDR  0x0800F800
#define FLASH_PAGE_NUMBER   31

void Flash_Save_BMS_State(BMS_State_t *bms);
void Flash_Load_BMS_State(BMS_State_t *bms);
#endif
