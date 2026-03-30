#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int display_model_to_json(const display_model_t *m, char *buf, size_t size)
{
    return snprintf(buf, size,
                    "{"
                    "\"version\":%d,"
                    "\"wifi\":%d,"
                    "\"ip\":\"%s\","
                    "\"cpu_temp\":%d,"
                    "\"cpu_load\":%d,"
                    "\"bmp280_temp\":%.2f,"
                    "\"bmp280_press\":%.2f,"
                    "\"htu21d_temp\":%.2f,"
                    "\"htu21d_hum\":%.2f"
                    "}",
                    DISPLAY_MODEL_VERSION, m->wifi_connected, m->ip, m->cpu_temp, m->cpu_load,
                    m->bmp280_temperature, m->bmp280_pressure, m->htu21d_temperature,
                    m->htu21d_humidity);
}

int display_model_from_json(const char *json, display_model_t *m)
{
    if (!json || !m)
        return -1;

    sscanf(json,
           "{"
           "\"version\":%*d,"
           "\"wifi\":%d,"
           "\"ip\":\"%31[^\"]\","
           "\"cpu_temp\":%d,"
           "\"cpu_load\":%d,"
           "\"bmp280_temp\":%f,"
           "\"bmp280_press\":%f,"
           "\"htu21d_temp\":%f,"
           "\"htu21d_hum\":%f"
           "}",
           (int *)&m->wifi_connected, m->ip, &m->cpu_temp, &m->cpu_load, &m->bmp280_temperature,
           &m->bmp280_pressure, &m->htu21d_temperature, &m->htu21d_humidity);

    return 0;
}