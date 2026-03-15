#ifndef HD44780_DISPLAY_H
#define HD44780_DISPLAY_H

/*
    1602 (16x2) LCD module with an HD44780 controller,
    connected through a PCF8574 I²C backpack.
*/
#include "i2c.h"
#include <stdint.h>

struct hd44780;

struct hd44780 *hd44780_create(struct I2cBus *i2c_bus);

int hd44780_init(struct hd44780 *self);

void hd44780_destroy(struct hd44780 *lcd);

/* Print a string on line 0 or 1 */
void hd44780_print(struct hd44780 *lcd, const char *str, uint8_t line);

/* Clear whole hd44780 */
void hd44780_clear(struct hd44780 *lcd);

#endif /* HD44780_DISPLAY_H */
