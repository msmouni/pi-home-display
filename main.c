#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "db.h"
#include "display/display.h"
#include "display/model.h"
#include "i2c.h"

#define I2C_BUS "/dev/i2c-1"
#define DB_FILE "/var/lib/pi-home-sensors_data/data.db"
#define DB_DATA_SIZE 100

volatile sig_atomic_t keep_running = 1; // Flag for shutdown

#define SENSOR_PERIOD_SEC 5
#define CLOCK_PERIOD_SEC 1

#define SENSORS_SIG SIGUSR1
#define CLOCK_SIG SIGUSR2

volatile sig_atomic_t sensor_tick = 0;
volatile sig_atomic_t clock_tick = 0;

void handle_signal(int signal)
{
    printf("\nCaught signal %d. Shutting down...\n", signal);
    keep_running = 0;
}

void sensor_handler(int sig)
{
    if (sig != SENSORS_SIG)
        return;
    sensor_tick = 1;
}

void clock_handler(int sig)
{
    if (sig != CLOCK_SIG)
        return;
    clock_tick = 1;
}

timer_t make_timer(int signo, __sighandler_t handler, int sec_period)
{

    signal(signo, handler);

    struct sigevent sev = {0};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = signo;

    timer_t timerid;
    timer_create(CLOCK_REALTIME, &sev, &timerid);

    struct itimerspec its = {0};
    its.it_value.tv_sec = sec_period;
    its.it_interval.tv_sec = sec_period;

    timer_settime(timerid, 0, &its, NULL);

    return timerid;
}

int get_ip_address(char *ip, size_t maxlen)
{
    struct ifaddrs *ifaddr, *ifa;

    // Get all network interfaces
    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;

        // IPv4
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;

            // Skip loopback
            if (sa->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                continue;

            inet_ntop(AF_INET, &sa->sin_addr, ip, maxlen);
            freeifaddrs(ifaddr);
            return 0;
        }
    }

    freeifaddrs(ifaddr);
    return -1;
}

void update_model_sensors(const sensors_sample_t *sample, display_model_t *model)
{
    if (sample->bmp280_temperature != -1 && sample->bmp280_pressure != -1)
    {
        model->has_bmp280 = true;
        model->bmp280_temperature = sample->bmp280_temperature;
        model->bmp280_pressure = sample->bmp280_pressure;
    }
    else
    {
        model->has_bmp280 = false;
    }

    if (sample->htu21d_temperature != -1 && sample->htu21d_humidity != -1)
    {
        model->has_htu21d = true;
        model->htu21d_temperature = sample->htu21d_temperature;
        model->htu21d_humidity = sample->htu21d_humidity;
    }
    else
    {
        model->has_htu21d = false;
    }
}

void update_model_ip(display_model_t *model)
{
    if (get_ip_address(model->ip, sizeof(model->ip)) != 0)
    {
        strncpy(model->ip, "no link", sizeof(model->ip));
    }
}

void update_model_timestamp(display_model_t *model)
{
    model->timestamp = time(NULL);
}

int main(void)
{
    int res = 0;

    // Set up signal handlers for SIGINT and SIGTERM
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    timer_t t_sensor = make_timer(SENSORS_SIG, sensor_handler, SENSOR_PERIOD_SEC);
    timer_t t_clock = make_timer(CLOCK_SIG, clock_handler, CLOCK_PERIOD_SEC);

    struct I2cBus *i2c_bus = i2c_init(I2C_BUS);
    if (!i2c_bus)
    {
        perror("Failed to initialize I2C bus");
        res = -1;
        goto err_i2c;
    }

    display_t *display = display_create(DISPLAY_OLED_128x32, i2c_bus);
    if (!display)
    {
        fprintf(stderr, "Failed to create OLED display\n");
        res = -1;
        goto err_display_create;
    }

    res = display_init(display);
    if (res != 0)
    {
        fprintf(stderr, "Failed to initialize display\n");
        res = -1;
        goto err_display_init;
    }

    display_model_t model = {0};

    // Initialize SQLite database
    struct sensors_db *sens_db = sensors_db_open(DB_FILE, SENSORS_DB_CONSUMER, DB_DATA_SIZE);
    if (!sens_db)
    {
        fprintf(stderr, "Failed to open database\n");
        keep_running = 0;
    }

    sensors_sample_t latest_sample;

    // Main measurement loop
    while (keep_running)
    {
        if (sensor_tick)
        {
            sensor_tick = 0;

            int res = sensors_db_read_latest(sens_db, &latest_sample);
            if (res != 0)
            {
                /* TODO: handle error message */
                continue;
            }

            update_model_sensors(&latest_sample, &model);

            update_model_ip(&model);
        }

        if (clock_tick)
        {
            clock_tick = 0;
            // get current time as a string and draw it on the OLED
            update_model_timestamp(&model);

            display_update(display, &model);
        }

        pause(); // sleep until next signal
    }

    if (sens_db)
        sensors_db_close(sens_db);

err_display_create:
    display_destroy(display);

err_display_init:
    i2c_close(i2c_bus);

err_i2c:
    // Cleanup before exiting
    timer_delete(t_sensor);
    timer_delete(t_clock);

    return res;
}
