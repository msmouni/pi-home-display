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
#include "lcd_16x2.h"
#include "oled_128x32.h"

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

void display_ip()
{
    char ip[INET_ADDRSTRLEN] = {0};

    if (get_ip_address(ip, sizeof(ip)) == 0)
    {
        char line[32];
        snprintf(line, sizeof(line), "IP:%s", ip);

        oled_128x32_draw_string(1, 0, line);
    }
    else
    {
        oled_128x32_draw_string(1, 0, "IP: no link");
    }
}

void print_sensor_data(float bmp280_temp, float bmp280_pressure,
                       float temperature, float humidity)
{
    char info_msg_l1[MAX_PRINT_SIZE] = {0};
    char info_msg_l2[MAX_PRINT_SIZE] = {0};

    lcd_16x2_clear();
    oled_128x32_clear_line(2);
    oled_128x32_clear_line(3);

    snprintf(info_msg_l1, MAX_PRINT_SIZE, "T=%.1fC|P=%dhPa", bmp280_temp, (int)bmp280_pressure);
    lcd_16x2_print(info_msg_l1, 0);
    oled_128x32_draw_string(2, 0, info_msg_l1);

    if (temperature != -1 && humidity != -1)
    {
        snprintf(info_msg_l2, MAX_PRINT_SIZE, "T=%.2fC|H=%d%%", temperature, (int)humidity);
        lcd_16x2_print(info_msg_l2, 1);
        oled_128x32_draw_string(3, 0, info_msg_l2);
    }
    else
    {
        snprintf(info_msg_l2, MAX_PRINT_SIZE, "HTU21D: Invalid data");
        lcd_16x2_print(info_msg_l2, 1);
        oled_128x32_draw_string(3, 0, info_msg_l2);
    }
}

void oled_display_time()
{
    time_t now = time(NULL);
    struct tm tm_now;
    char ts[32];

    localtime_r(&now, &tm_now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);
    oled_128x32_draw_string(0, 0, ts);
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
        goto out;
    }

    // OLED
    oled_128x32_init(i2c_bus);
    oled_128x32_clear();
    oled_128x32_draw_string(0, 0, "Hello from OLED!");

    lcd_16x2_create(i2c_bus);
    lcd_16x2_print("   Welcome to   ", 0);
    lcd_16x2_print("pi-home-sensors", 1);
    sleep(3);

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
                oled_128x32_draw_string(2, 0, "No sensor data");
                continue;
            }

            print_sensor_data(latest_sample.bmp280_temperature,
                              latest_sample.bmp280_pressure,
                              latest_sample.htu21d_temperature,
                              latest_sample.htu21d_humidity);

            display_ip();
        }

        if (clock_tick)
        {
            clock_tick = 0;
            // get current time as a string and draw it on the OLED
            oled_display_time();
        }

        pause(); // sleep until next signal
    }

out:
    // Cleanup before exiting
    timer_delete(t_sensor);
    timer_delete(t_clock);

    lcd_16x2_clear();
    oled_128x32_clear();

    sensors_db_close(sens_db);

    i2c_close(i2c_bus);

    return res;
}
