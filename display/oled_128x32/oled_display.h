#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"
#include "i2c.h"

display_t *oled_128x32_create(struct I2cBus *bus);

#endif /* OLED_DISPLAY_H */