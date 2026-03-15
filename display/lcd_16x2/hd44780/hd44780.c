/*
 * LCD 16x2 (HD44780 controller) over I²C (PCF8574 backpack)
 *
 * This example follows the HD44780 datasheet closely:
 * - 4-bit mode initialization (Section 10 "Initializing by Instruction")
 * - Uses PCF8574 I²C I/O expander (address typically 0x27 or 0x3F)
 * - Displays "Hello" and "World!" alternately.
 *
 * Wiring (via PCF8574 backpack):
 *   P0 → RS (Register Select)
 *   P1 → RW (Read/Write, always 0 for write)
 *   P2 → EN (Enable pulse)
 *   P3 → Backlight (1 = on)
 *   P4 → D4
 *   P5 → D5
 *   P6 → D6
 *   P7 → D7
 */

#include "hd44780.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i2c.h"
#include "low_level/low_level.h"
#include "model.h"

#define PCF8574_I2C_ADDR 0x27

#define MAX_CHARS 16
#define SCROLL_DELAY_US 500000

struct hd44780 {
    struct hd44780_ll *ll;

    char line1[MAX_PRINT_SIZE];
    char line2[MAX_PRINT_SIZE];

    int offset1;
    int offset2;

    _Atomic bool l1_update_needed;
    _Atomic bool l2_update_needed;

    _Atomic bool running;

    pthread_mutex_t lock;
    pthread_t thread;
};

/* Print a static (non-scrolling) line */
static void hd44780_print_line(struct hd44780 *disp, uint8_t line, const char *src, int offset)
{
    hd44780_ll_set_cursor(disp->ll, line);

    for (int i = 0; i < MAX_CHARS; i++) {
        char c = src[offset + i];

        if (c == '\0') // pad with spaces
            c = ' ';

        hd44780_ll_data(disp->ll, c);
    }
}

/* Circular scrolling with doubled buffer */
__attribute__((unused)) static void hd44780_print_circular(struct hd44780 *disp, uint8_t line,
                                                           const char *src, int *offset)
{
    int len = strlen(src);

    /* No need to scroll short text */
    if (len <= MAX_CHARS) {
        hd44780_print_line(disp, line, src, 0);
        return;
    }

    /* Create doubled string (circular buffer): text ->"texttext" */
    static char buf[2 * MAX_PRINT_SIZE];
    snprintf(buf, sizeof(buf), "%s%s", src, src);

    hd44780_ll_set_cursor(disp->ll, line);

    for (int i = 0; i < MAX_CHARS; i++)
        hd44780_ll_data(disp->ll, buf[*offset + i]);

    /* Advance offset circularly */
    *offset = (*offset + 1) % len;
}

/* Rollback scrolling */
static void hd44780_print_rollback(struct hd44780 *disp, uint8_t line, const char *src, int *offset)
{
    int len = strlen(src);

    /* Print substring */
    hd44780_print_line(disp, line, src, (len > MAX_CHARS) ? *offset : 0);

    /* Scroll */
    if (len > MAX_CHARS)
        *offset = (*offset + 1) % (len - MAX_CHARS + 1);
}

static void *hd44780_thread(void *arg)
{
    struct hd44780 *disp = (struct hd44780 *)arg;

    while (atomic_load(&disp->running)) {
        pthread_mutex_lock(&disp->lock);

        /* Reset offsets when content changes */
        if (atomic_exchange(&disp->l1_update_needed, false)) {
            disp->offset1 = 0;
            hd44780_ll_clear(disp->ll);
        } else if (atomic_exchange(&disp->l2_update_needed, false)) {
            disp->offset2 = 0;
            hd44780_ll_clear(disp->ll);
        }

        /* Print both lines + Scroll */
        hd44780_print_rollback(disp, 0, disp->line1, &disp->offset1);
        hd44780_print_rollback(disp, 1, disp->line2, &disp->offset2);

        pthread_mutex_unlock(&disp->lock);

        usleep(SCROLL_DELAY_US);
    }

    return NULL;
}

struct hd44780 *hd44780_create(struct I2cBus *i2c_bus)
{
    struct hd44780 *disp = calloc(1, sizeof(struct hd44780));
    if (!disp)
        return NULL;

    disp->ll = hd44780_ll_create(i2c_bus, PCF8574_I2C_ADDR);
    if (!disp->ll) {
        free(disp);
        return NULL;
    }

    return disp;
}

int hd44780_init(struct hd44780 *self)
{
    int ret;

    if (!self || !self->ll)
        return -1;

    ret = pthread_mutex_init(&self->lock, NULL);
    if (ret != 0)
        return ret;

    atomic_store(&self->running, true);
    ret = pthread_create(&self->thread, NULL, hd44780_thread, self);

    return ret;
}

void hd44780_destroy(struct hd44780 *disp)
{
    atomic_store(&disp->running, false);
    pthread_join(disp->thread, NULL);
    pthread_mutex_destroy(&disp->lock);
}

void hd44780_print(struct hd44780 *disp, const char *str, uint8_t line)
{
    if (!str || line > 1)
        return;

    pthread_mutex_lock(&disp->lock);

    if (line == 0) {
        strncpy(disp->line1, str, MAX_PRINT_SIZE - 1);
        atomic_store(&disp->l1_update_needed, true);
    } else {
        strncpy(disp->line2, str, MAX_PRINT_SIZE - 1);
        atomic_store(&disp->l2_update_needed, true);
    }

    pthread_mutex_unlock(&disp->lock);
}

void hd44780_clear(struct hd44780 *disp)
{
    pthread_mutex_lock(&disp->lock);
    disp->line1[0] = '\0';
    disp->line2[0] = '\0';
    atomic_store(&disp->l1_update_needed, true);
    atomic_store(&disp->l2_update_needed, true);
    pthread_mutex_unlock(&disp->lock);
}
