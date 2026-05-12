#ifndef I2C_LCD_H
#define I2C_LCD_H

/*
 * Simple I2C LCD helper used by the plotter menu.
 */

#include <stdint.h>

/* Pick the HAL header that matches the MCU family. */
#if __has_include("stm32f3xx_hal.h")
    #include "stm32f3xx_hal.h"
#elif __has_include("stm32f1xx_hal.h")
    #include "stm32f1xx_hal.h"
#elif __has_include("stm32f4xx_hal.h")
    #include "stm32f4xx_hal.h"
#elif __has_include("stm32c0xx_hal.h")
    #include "stm32c0xx_hal.h"
#elif __has_include("stm32g4xx_hal.h")
    #include "stm32g4xx_hal.h"
#endif

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t address;
} I2C_LCD_HandleTypeDef;

void lcd_init(I2C_LCD_HandleTypeDef *lcd);
void lcd_send_cmd(I2C_LCD_HandleTypeDef *lcd, char cmd);
void lcd_send_data(I2C_LCD_HandleTypeDef *lcd, char data);
void lcd_putchar(I2C_LCD_HandleTypeDef *lcd, char ch);
void lcd_puts(I2C_LCD_HandleTypeDef *lcd, char *str);
void lcd_gotoxy(I2C_LCD_HandleTypeDef *lcd, int col, int row);
void lcd_clear(I2C_LCD_HandleTypeDef *lcd);

#endif /* I2C_LCD_H */
