#ifndef PI_HOME_SENSORS_DISPLAY_LOW_LEVEL_H
#define PI_HOME_SENSORS_DISPLAY_LOW_LEVEL_H

/* LCD 16x2 (HD44780 controller) over I²C (PCF8574 backpack) */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "i2c.h"

/* Initialize LCD in 4-bit mode (datasheet Figure 24) */
void lcd_16x2_ll_init(struct I2cBus *i2c_bus, uint8_t i2c_addr);

/* Set cursor to line (1 or 2) */
void lcd_16x2_ll_set_cursor(uint8_t line);

/* LCD data (character) */
void lcd_16x2_ll_data(uint8_t data);

/* Clear the lcd_16x2 */
void lcd_16x2_ll_clear(void);

#endif /* PI_HOME_SENSORS_DISPLAY_LOW_LEVEL_H */
