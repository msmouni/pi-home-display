#ifndef LCD_16X2_DISPLAY_H
#define LCD_16X2_DISPLAY_H

#include "display.h"
#include "i2c.h"

display_t *lcd_16x2_create(struct I2cBus *bus);

#endif /* LCD_16X2_DISPLAY_H */
