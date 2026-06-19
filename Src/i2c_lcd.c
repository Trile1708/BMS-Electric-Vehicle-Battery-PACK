#include "i2c_lcd.h"

extern I2C_HandleTypeDef hi2c2;

/*
   PCF8574 mapping ph? bi?n:
   P0 = RS
   P1 = RW
   P2 = EN
   P3 = Backlight
   P4 = D4
   P5 = D5
   P6 = D6
   P7 = D7
*/

#define LCD_RS          0x01
#define LCD_RW          0x02
#define LCD_EN          0x04
#define LCD_BACKLIGHT   0x08

static uint8_t lcd_backlight = LCD_BACKLIGHT;

static void lcd_i2c_write(uint8_t data)
{
    HAL_I2C_Master_Transmit(&hi2c2, LCD_I2C_ADDR, &data, 1, 100);
}

static void lcd_pulse_enable(uint8_t data)
{
    lcd_i2c_write(data | LCD_EN);
    HAL_Delay(1);

    lcd_i2c_write(data & ~LCD_EN);
    HAL_Delay(1);
}

/*
   G?i dúng 1 nibble 4-bit.
   nibble ph?i n?m ? 4 bit cao: 0x30, 0x20, 0x80...
*/
static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data;

    data = (nibble & 0xF0) | lcd_backlight;

    if (rs)
        data |= LCD_RS;

    lcd_i2c_write(data);
    lcd_pulse_enable(data);
}

/*
   G?i d? 1 byte l?nh ho?c d? li?u:
   byte cao tru?c, byte th?p sau.
*/
static void lcd_send_byte(uint8_t value, uint8_t rs)
{
    lcd_write_nibble(value & 0xF0, rs);
    lcd_write_nibble((value << 4) & 0xF0, rs);
}

void lcd_send_cmd(char cmd)
{
    lcd_send_byte((uint8_t)cmd, 0);
}

void lcd_send_data(char data)
{
    lcd_send_byte((uint8_t)data, 1);
}

void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);
    }
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

/*
   LCD 20x04 HD44780 DDRAM address:
   Row 0: 0x00
   Row 1: 0x40
   Row 2: 0x14
   Row 3: 0x54
*/
void lcd_goto_XY(int row, int col)
{
    uint8_t row_addr[4] = {0x00, 0x40, 0x14, 0x54};

    if (row < 0)
        row = 0;

    if (row > 3)
        row = 3;

    if (col < 0)
        col = 0;

    if (col > 19)
        col = 19;

    lcd_send_cmd(0x80 | (row_addr[row] + col));
}

void lcd_init(void)
{
    HAL_Delay(100);

    /*
       Kh?i t?o LCD HD44780 dúng chu?n 4-bit.
       Giai do?n này ch? g?i nibble cao, chua g?i d? byte.
    */
    lcd_write_nibble(0x30, 0);
    HAL_Delay(5);

    lcd_write_nibble(0x30, 0);
    HAL_Delay(5);

    lcd_write_nibble(0x30, 0);
    HAL_Delay(5);

    lcd_write_nibble(0x20, 0);
    HAL_Delay(5);

    /*
       4-bit, 2-line mode.
       LCD 20x04 v?n dùng l?nh 0x28.
    */
    lcd_send_cmd(0x28);

    /*
       Display ON, cursor OFF, blink OFF
    */
    lcd_send_cmd(0x0C);

    /*
       Entry mode: t? tang con tr?
    */
    lcd_send_cmd(0x06);

    lcd_clear();

    lcd_goto_XY(0, 0);
    lcd_send_string("LCD 20x04 OK");
}