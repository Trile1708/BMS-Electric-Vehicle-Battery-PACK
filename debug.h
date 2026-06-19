#ifndef DEBUG_H
#define DEBUG_H

#include "main.h"

extern volatile uint8_t Debug_SYS1;
extern volatile uint8_t Debug_SYS2;

extern volatile uint8_t Debug_HardwareFault;
extern volatile uint8_t Debug_SoftwareFault;

extern volatile uint32_t Debug_PackVoltage;
extern volatile int16_t Debug_Current;
extern volatile uint8_t Debug_SYSCTRL1;
extern volatile uint8_t Debug_SYSCTRL2;
extern volatile uint8_t Debug_PROTECT1;
extern volatile uint8_t Debug_PROTECT2;
extern volatile uint8_t Debug_PROTECT3;
extern volatile float Debug_Temp;
extern volatile uint8_t Debug_I2C_Write_OK;
extern volatile uint16_t Debug_Cell[15];
extern volatile uint8_t Debug_OV_TRIP ;
extern volatile uint8_t Debug_UV_TRIP ;
extern volatile uint8_t Debug_CC_CFG ;
#endif
