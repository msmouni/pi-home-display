#ifndef DISPLAY_H
#define DISPLAY_H

#include "model.h"
#include <stdbool.h>

typedef enum display_type {
    DISPLAY_OLED_128x32,
    DISPLAY_LCD_16x2,
} display_type_t;

typedef struct display_ops {
    int (*init)(void *ctx);
    void (*clear)(void *ctx);
    void (*update)(void *ctx, const display_model_t *model);
    void (*destroy)(void *ctx);
} display_ops_t;

typedef struct display {
    display_type_t type;

    void *ctx;
    const display_ops_t *ops;
} display_t;

display_t *display_create(display_type_t type, void *cfg);
int display_init(display_t *disp);
void display_clear(display_t *disp);
void display_update(display_t *disp, const display_model_t *model);
void display_destroy(display_t *disp);

#endif /* DISPLAY_H */
