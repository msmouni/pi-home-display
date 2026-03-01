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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "lcd_16x2.h"
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include "i2c.h"
#include "lcd_16x2/low_level/low_level.h"

#define PCF8574_I2C_ADDR 0x27

#define MAX_CHARS 16
#define SCROLL_DELAY_US 500000

typedef struct
{
    char line1[MAX_PRINT_SIZE];
    char line2[MAX_PRINT_SIZE];

    int offset1;
    int offset2;

    _Atomic bool l1_update_needed;
    _Atomic bool l2_update_needed;

    _Atomic bool running;

    pthread_mutex_t lock;
    pthread_t thread;
} lcd_16x2_t;

static lcd_16x2_t lcd_16x2;

/* Print a static (non-scrolling) line */
static void lcd_16x2_print_line(uint8_t line, const char *src, int offset)
{
    lcd_16x2_ll_set_cursor(line);

    for (int i = 0; i < MAX_CHARS; i++)
    {
        char c = src[offset + i];

        if (c == '\0') // pad with spaces
            c = ' ';

        lcd_16x2_ll_data(c);
    }
}

/* Circular scrolling with doubled buffer */
__attribute__((unused)) static void lcd_16x2_print_circular(uint8_t line, const char *src, int *offset)
{
    int len = strlen(src);

    /* No need to scroll short text */
    if (len <= MAX_CHARS)
    {
        lcd_16x2_print_line(line, src, 0);
        return;
    }

    /* Create doubled string (circular buffer): text ->"texttext" */
    static char buf[2 * MAX_PRINT_SIZE];
    snprintf(buf, sizeof(buf), "%s%s", src, src);

    lcd_16x2_ll_set_cursor(line);

    for (int i = 0; i < MAX_CHARS; i++)
        lcd_16x2_ll_data(buf[*offset + i]);

    /* Advance offset circularly */
    *offset = (*offset + 1) % len;
}

/* Rollback scrolling */
static void lcd_16x2_print_rollback(uint8_t line, const char *src, int *offset)
{
    int len = strlen(src);

    /* Print substring */
    lcd_16x2_print_line(line, src,
                        (len > MAX_CHARS) ? *offset : 0);

    /* Scroll */
    if (len > MAX_CHARS)
        *offset = (*offset + 1) % (len - MAX_CHARS + 1);
}

static void *lcd_16x2_thread(void *arg)
{
    (void)arg;

    while (atomic_load(&lcd_16x2.running))
    {
        pthread_mutex_lock(&lcd_16x2.lock);

        /* Reset offsets when content changes */
        if (atomic_exchange(&lcd_16x2.l1_update_needed, false))
        {
            lcd_16x2.offset1 = 0;
            lcd_16x2_ll_clear();
        }
        else if (atomic_exchange(&lcd_16x2.l2_update_needed, false))
        {
            lcd_16x2.offset2 = 0;
            lcd_16x2_ll_clear();
        }

        /* Print both lines + Scroll */
        lcd_16x2_print_rollback(0, lcd_16x2.line1, &lcd_16x2.offset1);
        lcd_16x2_print_rollback(1, lcd_16x2.line2, &lcd_16x2.offset2);

        pthread_mutex_unlock(&lcd_16x2.lock);

        usleep(SCROLL_DELAY_US);
    }

    return NULL;
}

void lcd_16x2_create(struct I2cBus *i2c_bus)
{
    memset(&lcd_16x2, 0, sizeof(lcd_16x2));
    pthread_mutex_init(&lcd_16x2.lock, NULL);

    lcd_16x2_ll_init(i2c_bus, PCF8574_I2C_ADDR);

    atomic_store(&lcd_16x2.running, true);
    pthread_create(&lcd_16x2.thread, NULL, lcd_16x2_thread, NULL);
}

void lcd_16x2_destroy(void)
{
    atomic_store(&lcd_16x2.running, false);
    pthread_join(lcd_16x2.thread, NULL);
    pthread_mutex_destroy(&lcd_16x2.lock);
}

void lcd_16x2_print(const char *str, uint8_t line)
{
    if (!str || line > 1)
        return;

    pthread_mutex_lock(&lcd_16x2.lock);

    if (line == 0)
    {
        strncpy(lcd_16x2.line1, str, MAX_PRINT_SIZE - 1);
        atomic_store(&lcd_16x2.l1_update_needed, true);
    }
    else
    {
        strncpy(lcd_16x2.line2, str, MAX_PRINT_SIZE - 1);
        atomic_store(&lcd_16x2.l2_update_needed, true);
    }

    pthread_mutex_unlock(&lcd_16x2.lock);
}

void lcd_16x2_clear(void)
{
    pthread_mutex_lock(&lcd_16x2.lock);
    lcd_16x2.line1[0] = '\0';
    lcd_16x2.line2[0] = '\0';
    atomic_store(&lcd_16x2.l1_update_needed, true);
    atomic_store(&lcd_16x2.l2_update_needed, true);
    pthread_mutex_unlock(&lcd_16x2.lock);
}
