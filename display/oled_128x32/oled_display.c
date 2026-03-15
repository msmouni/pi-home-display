#include <stdlib.h>
#include "oled_display.h"
#include "ssd1306.h"

struct oled_ctx
{
    struct ssd1306 *dev;

    char line_1[MAX_PRINT_SIZE];
    char line_2[MAX_PRINT_SIZE];
    char line_3[MAX_PRINT_SIZE];
    char line_4[MAX_PRINT_SIZE];
};

static int oled_init(void *ctx)
{
    if (!ctx)
        return -1;

    struct oled_ctx *oled = ctx;
    int ret = ssd1306_init(oled->dev);
    if (ret)
        return ret;

    ssd1306_clear(oled->dev);
    ssd1306_draw_string(oled->dev, 0, 0, "Hello from OLED!");

    return 0;
}

static void oled_clear(void *ctx)
{
    if (!ctx)
        return;

    struct oled_ctx *oled = ctx;
    ssd1306_clear(oled->dev);
}

static void oled_update(void *ctx, const display_model_t *model)
{
    if (!ctx)
        return;

    struct oled_ctx *oled = ctx;

    /* Time */
    struct tm timeinfo;
    localtime_r(&model->timestamp, &timeinfo);
    strftime(oled->line_1, sizeof(oled->line_1), "%Y-%m-%d %H:%M:%S", &timeinfo);

    /* IP Address */
    snprintf(oled->line_2, sizeof(oled->line_2), "IP:%s", model->ip);

    /* Sensor Data */
    snprintf(oled->line_3, sizeof(oled->line_3), "T=%.1fC|P=%dhPa", model->bmp280_temperature, (int)model->bmp280_pressure);

    if (model->htu21d_temperature != -1 && model->htu21d_humidity != -1)
    {
        snprintf(oled->line_4, sizeof(oled->line_4), "T=%.2fC|H=%d%%", model->htu21d_temperature, (int)model->htu21d_humidity);
    }
    else
    {
        snprintf(oled->line_4, sizeof(oled->line_4), "HTU21D: Invalid data");
    }

    ssd1306_draw_line(oled->dev, 0, oled->line_1);
    ssd1306_draw_line(oled->dev, 1, oled->line_2);
    ssd1306_draw_line(oled->dev, 2, oled->line_3);
    ssd1306_draw_line(oled->dev, 3, oled->line_4);
}

static void oled_destroy(void *ctx)
{
    if (!ctx)
        return;

    struct oled_ctx *oled = ctx;

    ssd1306_destroy(oled->dev);
    free(oled);
}

static const display_ops_t oled_ops = {
    .init = oled_init,
    .clear = oled_clear,
    .update = oled_update,
    .destroy = oled_destroy,
};

display_t *oled_128x32_create(struct I2cBus *bus)
{
    display_t *disp = calloc(1, sizeof(display_t));
    if (!disp)
        return NULL;

    struct oled_ctx *ctx = malloc(sizeof(struct oled_ctx));
    if (!ctx)
    {
        free(disp);
        return NULL;
    }

    ctx->dev = ssd1306_create(bus);

    disp->type = DISPLAY_OLED_128x32;
    disp->ctx = ctx;
    disp->ops = &oled_ops;

    return disp;
}