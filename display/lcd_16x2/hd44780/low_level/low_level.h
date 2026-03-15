#ifndef HD44780_LOW_LEVEL_H
#define HD44780_LOW_LEVEL_H

/* LCD 16x2 (HD44780 controller) over I²C (PCF8574 backpack) */

#include "i2c.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct hd44780_ll;

/* Initialize LCD in 4-bit mode (datasheet Figure 24) */
struct hd44780_ll *hd44780_ll_create(struct I2cBus *i2c_bus, uint8_t i2c_addr);

/* Set cursor to line (1 or 2) */
void hd44780_ll_set_cursor(struct hd44780_ll *self, uint8_t line);

/* LCD data (character) */
void hd44780_ll_data(struct hd44780_ll *self, uint8_t data);

/* Clear the hd44780 */
void hd44780_ll_clear(struct hd44780_ll *self);

#endif /* HD44780_LOW_LEVEL_H */
