#ifndef I2C_LCD_H
#define I2C_LCD_H

#include "main.h"

// ----------------------------------------------------
// ÐIA CHI I2C CUA MODULE LCD
// ----------------------------------------------------
#define LCD_I2C_ADDR 0x4E 

// Khai báo các hàm giao tiep
void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_goto_XY(int row, int col);
void lcd_clear(void);
#endif
