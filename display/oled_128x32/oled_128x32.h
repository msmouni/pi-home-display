#ifndef PI_HOME_SENSORS_OLED_128x32_H
#define PI_HOME_SENSORS_OLED_128x32_H

/* C driver for the SSD1306 0.91" OLED (128×32) over I²C */

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32

int oled_128x32_init(struct I2cBus *i2c_bus);

void oled_128x32_clear(void);

void oled_128x32_set_cursor(uint8_t page, uint8_t column);
void oled_128x32_draw_char(char c);
void oled_128x32_draw_string(uint8_t page, uint8_t col, const char *str);
void oled_128x32_clear_line(uint8_t page);

void oled_128x32_close();

#endif /* PI_HOME_SENSORS_OLED_128x32_H */
