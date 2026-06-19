#ifndef APP_BMS_H
#define APP_BMS_H
#include "main.h"
#include "bq76940.h"

void BMS_Update_LED_Status(BMS_State_t *bms);
void BMS_Auto_Recovery(BMS_State_t *bms);

#endif /* APP_BMS_H */
