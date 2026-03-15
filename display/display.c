#include "display.h"
#include "lcd_16x2/lcd_16x2_display.h"
#include "model.h"
#include "oled_128x32/oled_display.h"
#include <stdlib.h>

display_t *display_create(display_type_t type, void *cfg)
{
    display_t *disp;

    switch (type) {
    case DISPLAY_OLED_128x32:
        disp = oled_128x32_create((struct I2cBus *)cfg);
        break;
    case DISPLAY_LCD_16x2:
        disp = lcd_16x2_create((struct I2cBus *)cfg);
        break;
    default:
        return NULL;
    }

    return disp;
}

int display_init(display_t *disp)
{
    if (!disp || !disp->ops || !disp->ops->init)
        return -1;

    return disp->ops->init(disp->ctx);
}

void display_clear(display_t *disp)
{
    if (!disp || !disp->ops || !disp->ops->clear)
        return;

    disp->ops->clear(disp->ctx);
}

void display_update(display_t *disp, const display_model_t *model)
{
    if (!disp || !disp->ops || !disp->ops->update)
        return;

    disp->ops->update(disp->ctx, model);
}

void display_destroy(display_t *disp)
{
    if (!disp || !disp->ops || !disp->ops->destroy)
        return;

    disp->ops->destroy(disp->ctx);
    free(disp);
}
