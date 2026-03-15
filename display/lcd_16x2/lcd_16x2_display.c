#include "lcd_16x2_display.h"
#include "hd44780.h"
#include <stdlib.h>

struct lcd_16x2_ctx {
    struct hd44780 *dev;

    char line_1[MAX_PRINT_SIZE];
    char line_2[MAX_PRINT_SIZE];
};

static int lcd_16x2_init(void *ctx)
{
    if (!ctx)
        return -1;

    struct lcd_16x2_ctx *lcd = ctx;
    int ret = hd44780_init(lcd->dev);
    if (ret)
        return ret;

    hd44780_clear(lcd->dev);
    hd44780_print(lcd->dev, "Hello from LCD!", 0);

    return 0;
}

static void lcd_16x2_clear(void *ctx)
{
    if (!ctx)
        return;

    struct lcd_16x2_ctx *lcd = ctx;
    hd44780_clear(lcd->dev);
}

static void lcd_16x2_update(void *ctx, const display_model_t *model)
{
    if (!ctx)
        return;

    struct lcd_16x2_ctx *lcd = ctx;

    /* Time + IP Address  */
    struct tm timeinfo;
    localtime_r(&model->timestamp, &timeinfo);
    char time[32] = {0};
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", &timeinfo);

    snprintf(lcd->line_1, sizeof(lcd->line_1), "%s | IP:%s", time, model->ip);

    /* Sensor Data */
    char htu21d[32] = {0};
    if (model->htu21d_temperature != -1 && model->htu21d_humidity != -1) {
        snprintf(htu21d, sizeof(htu21d), "T=%.2fC|H=%d%%", model->htu21d_temperature,
                 (int)model->htu21d_humidity);
    } else {
        snprintf(htu21d, sizeof(htu21d), "HTU21D: Invalid data");
    }

    snprintf(lcd->line_2, sizeof(lcd->line_2), "BMP280:T=%.1fC|P=%dhPa | HTU21D: %s",
             model->bmp280_temperature, (int)model->bmp280_pressure, htu21d);

    hd44780_print(lcd->dev, lcd->line_1, 0);
    hd44780_print(lcd->dev, lcd->line_2, 1);
}

static void lcd_16x2_destroy(void *ctx)
{
    if (!ctx)
        return;

    struct lcd_16x2_ctx *lcd = ctx;

    hd44780_destroy(lcd->dev);
    free(lcd);
}

static const display_ops_t lcd_16x2_ops = {
    .init = lcd_16x2_init,
    .clear = lcd_16x2_clear,
    .update = lcd_16x2_update,
    .destroy = lcd_16x2_destroy,
};

display_t *lcd_16x2_create(struct I2cBus *bus)
{
    display_t *disp = calloc(1, sizeof(display_t));
    if (!disp)
        return NULL;

    struct lcd_16x2_ctx *ctx = malloc(sizeof(struct lcd_16x2_ctx));
    if (!ctx) {
        free(disp);
        return NULL;
    }

    ctx->dev = hd44780_create(bus);

    disp->type = DISPLAY_LCD_16x2;
    disp->ctx = ctx;
    disp->ops = &lcd_16x2_ops;

    return disp;
}
