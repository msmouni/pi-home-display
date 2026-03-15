#ifndef DISPLAY_MODEL_H
#define DISPLAY_MODEL_H

#include <stdbool.h>
#include <time.h>

#define MAX_PRINT_SIZE 128
#define DISPLAY_MAX_IP_LEN 32

typedef struct
{
    /* Sensors data */
    bool has_bmp280;
    float bmp280_temperature;
    float bmp280_pressure;

    bool has_htu21d;
    float htu21d_temperature;
    float htu21d_humidity;

    /* System info */
    char ip[DISPLAY_MAX_IP_LEN];
    time_t timestamp;
} display_model_t;

#endif /* DISPLAY_MODEL_H */
