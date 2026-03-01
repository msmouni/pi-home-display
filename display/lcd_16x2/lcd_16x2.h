#ifndef PI_HOME_SENSORS_LCD_16x2_H
#define PI_HOME_SENSORS_LCD_16x2_H

/*
    1602 (16x2) LCD module with an HD44780 controller,
    connected through a PCF8574 I²C backpack.
*/
#include <stdint.h>
#include "i2c.h"

#define MAX_PRINT_SIZE 128

void lcd_16x2_create(struct I2cBus *i2c_bus);
void lcd_16x2_destroy(void);

/* Print a string on line 0 or 1 */
void lcd_16x2_print(const char *str, uint8_t line);

/* Clear whole lcd_16x2 */
void lcd_16x2_clear(void);

#endif /* PI_HOME_SENSORS_LCD_16x2_H */