#include "debug.h"
#include "bq76940.h"
volatile uint8_t Debug_SYS1=0;
volatile uint8_t Debug_SYS2=0;

volatile uint8_t Debug_HardwareFault=0;
volatile uint8_t Debug_SoftwareFault=0;


volatile uint32_t Debug_PackVoltage=0;
volatile int16_t Debug_Current=0;

volatile float Debug_Temp=0;

volatile uint16_t Debug_Cell[15]={0};
volatile uint8_t Debug_SYSCTRL1=0;
volatile uint8_t Debug_SYSCTRL2=0;
volatile uint8_t Debug_PROTECT1=0;
volatile uint8_t Debug_PROTECT2=0;
volatile uint8_t Debug_PROTECT3=0;
volatile uint8_t Debug_I2C_Write_OK=0;;
volatile uint8_t Debug_OV_TRIP = 0;
volatile uint8_t Debug_UV_TRIP = 0;
volatile uint8_t Debug_CC_CFG = 0;
