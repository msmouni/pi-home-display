#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "display/display.h"
#include "i2c.h"
#include "model.h"

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
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;

        // IPv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
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

int get_wifi_info(bool *connected, char *ssid, size_t ssid_len)
{
    FILE *fp;

    *connected = false;
    snprintf(ssid, ssid_len, "unknown");

    // Check if wlan0 is up
    fp = fopen("/sys/class/net/wlan0/operstate", "r");
    if (!fp)
        return -1;

    char state[16] = {0};
    fgets(state, sizeof(state), fp);
    fclose(fp);

    if (strncmp(state, "up", 2) != 0)
        return 0;

    *connected = true;

    // Try to get SSID (requires iw)
    fp = popen("iw dev wlan0 link 2>/dev/null", "r");
    if (!fp)
        return 0;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char *s = strstr(line, "SSID:");
        if (s) {
            s += 5; // Move past "SSID:"
            while (*s == ' ')
                s++;

            strncpy(ssid, s, ssid_len - 1);
            ssid[ssid_len - 1] = '\0';

            // remove newline
            ssid[strcspn(ssid, "\n")] = '\0';
            break;
        }
    }

    pclose(fp);
    return 0;
}

int get_cpu_temp(void)
{
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp)
        return -1;

    int temp;
    fscanf(fp, "%d", &temp);
    fclose(fp);

    return temp / 1000; // convert milli°C to °C
}

int get_cpu_load(void)
{
    /* https://www.man7.org/linux/man-pages/man5/proc_loadavg.5.html */
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp)
        return -1;

    float load;
    fscanf(fp, "%f", &load);
    fclose(fp);

    return (int)(load * 100); // rough %
}

int get_ram_usage(long *ram_total, int *ram_usage_pr)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;

    long total = 0;
    long available = 0;

    char key[64];
    long value;
    char unit[16];

    while (fscanf(fp, "%63s %ld %15s\n", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0)
            total = value;
        else if (strcmp(key, "MemAvailable:") == 0)
            available = value;

        if (total && available)
            break;
    }

    fclose(fp);

    if (total == 0)
        return -1;

    *ram_total = total / 1024; // convert kB to MB

    long used = total - available;
    *ram_usage_pr = (int)((used * 100) / total); // percentage

    return 0;
}

int get_uptime(long *uptime)
{
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp)
        return -1;

    double uptime_seconds = 0.0;

    if (fscanf(fp, "%lf", &uptime_seconds) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    *uptime = (long)uptime_seconds;
    return 0;
}

/* TODO: move to model module */
void update_model_system_info(display_model_t *model)
{
    if (!model)
        return;

    // IP
    if (get_ip_address(model->ip, sizeof(model->ip)) != 0) {
        strncpy(model->ip, "no link", sizeof(model->ip));
    }

    // WiFi
    get_wifi_info(&model->wifi_connected, model->wifi_ssid, sizeof(model->wifi_ssid));

    // CPU temp
    model->cpu_temp = get_cpu_temp();

    // CPU load
    model->cpu_load = get_cpu_load();

    // RAM usage
    get_ram_usage(&model->ram_total, &model->ram_usage_pr);

    // Uptime
    get_uptime(&model->uptime);
}

void update_model_timestamp(display_model_t *model) { model->timestamp = time(NULL); }

void update_model_sensors(const sensors_sample_t *sample, display_model_t *model)
{
    if (sample->bmp280_temperature != -1 && sample->bmp280_pressure != -1) {
        model->has_bmp280 = true;
        model->bmp280_temperature = sample->bmp280_temperature;
        model->bmp280_pressure = sample->bmp280_pressure;
    } else {
        model->has_bmp280 = false;
    }

    if (sample->htu21d_temperature != -1 && sample->htu21d_humidity != -1) {
        model->has_htu21d = true;
        model->htu21d_temperature = sample->htu21d_temperature;
        model->htu21d_humidity = sample->htu21d_humidity;
    } else {
        model->has_htu21d = false;
    }
}

void update_model_ip(display_model_t *model)
{
    if (get_ip_address(model->ip, sizeof(model->ip)) != 0) {
        strncpy(model->ip, "no link", sizeof(model->ip));
    }
}

void update_model_timestamp(display_model_t *model) { model->timestamp = time(NULL); }

int main(void)
{
    int res = 0;

    // Set up signal handlers for SIGINT and SIGTERM
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    timer_t t_sensor = make_timer(SENSORS_SIG, sensor_handler, SENSOR_PERIOD_SEC);
    timer_t t_clock = make_timer(CLOCK_SIG, clock_handler, CLOCK_PERIOD_SEC);

    /* TODO: parse config file */
    display_t *display = display_create(DISPLAY_LAFVIN, "/tmp/lafvin_display.sock");
    if (!display) {
        fprintf(stderr, "Failed to create LAFVIN display\n");
        res = -1;
        goto err_display_create;
    }

    res = display_init(display);
    if (res != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        res = -1;
        goto err_display_init;
    }

    display_model_t model = {0};

    // Initialize SQLite database
    struct sensors_db *sens_db = sensors_db_open(DB_FILE, SENSORS_DB_CONSUMER, DB_DATA_SIZE);
    if (!sens_db) {
        fprintf(stderr, "Failed to open database\n");
        keep_running = 0;
    }

    sensors_sample_t latest_sample;

    // Main measurement loop
    while (keep_running) {
        if (sensor_tick) {
            sensor_tick = 0;

            int res = sensors_db_read_latest(sens_db, &latest_sample);
            if (!res) {
                update_model_sensors(&latest_sample, &model);
            } else {
                fprintf(stderr, "Failed to read latest sensor data from database\n");
            }

            update_model_system_info(&model);
        }

        if (clock_tick) {
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
    // Cleanup before exiting
    timer_delete(t_sensor);
    timer_delete(t_clock);

    return res;
}
