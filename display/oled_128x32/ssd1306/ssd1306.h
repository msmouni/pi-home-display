#ifndef SSD1306_H
#define SSD1306_H

/* C driver for the SSD1306 0.91" OLED (128×32) over I²C */

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

struct ssd1306;

struct ssd1306 *ssd1306_create(struct I2cBus *i2c_bus);
int ssd1306_init(struct ssd1306 *self);

void ssd1306_clear(struct ssd1306 *self);

void ssd1306_set_cursor(struct ssd1306 *self, uint8_t page, uint8_t column);
void ssd1306_draw_char(struct ssd1306 *self, char c);
void ssd1306_draw_string(struct ssd1306 *self, uint8_t page, uint8_t col, const char *str);
void ssd1306_draw_line(struct ssd1306 *self, uint8_t page, const char *str);
void ssd1306_clear_line(struct ssd1306 *self, uint8_t page);

void ssd1306_destroy(struct ssd1306 *self);

#endif /* SSD1306_H */
