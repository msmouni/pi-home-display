/*
 * LCD 16x2 (HD44780 controller) over I²C (PCF8574 backpack)
 *
 * Following the HD44780 datasheet:
 * - 4-bit mode initialization (Section 10 "Initializing by Instruction")
 * - Using PCF8574 I²C I/O expander (address typically 0x27 or 0x3F)
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

#include "low_level/low_level.h"
#include "i2c.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* LCD command definitions (from HD44780U datasheet, Table 6) */
#define LCD_CLEAR_DISPLAY 0x01
#define LCD_RETURN_HOME 0x02
#define LCD_ENTRY_MODE_SET 0x04
#define LCD_DISPLAY_CONTROL 0x08
#define LCD_CURSOR_SHIFT 0x10
#define LCD_FUNCTION_SET 0x20
#define LCD_SET_CGRAM_ADDR 0x40
#define LCD_SET_DDRAM_ADDR 0x80

/* Flags for entry mode */
#define LCD_ENTRY_RIGHT 0x00
#define LCD_ENTRY_LEFT 0x02
#define LCD_ENTRY_SHIFT_INC 0x01
#define LCD_ENTRY_SHIFT_DEC 0x00

/* Flags for hd44780 on/off control */
#define LCD_DISPLAY_ON 0x04
#define LCD_DISPLAY_OFF 0x00
#define LCD_CURSOR_ON 0x02
#define LCD_CURSOR_OFF 0x00
#define LCD_BLINK_ON 0x01
#define LCD_BLINK_OFF 0x00

/* Flags for hd44780/cursor shift */
#define LCD_DISPLAY_MOVE 0x08
#define LCD_CURSOR_MOVE 0x00
#define LCD_MOVE_RIGHT 0x04
#define LCD_MOVE_LEFT 0x00

/* Flags for function set */
#define LCD_8BIT_MODE 0x10
#define LCD_4BIT_MODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

/* PCF8574 bit positions */
#define PIN_RS 0x01 /* P0 */
#define PIN_RW 0x02 /* P1 */
#define PIN_EN 0x04 /* P2 */
#define PIN_BL 0x08 /* P3 (backlight) */
#define BACKLIGHT 0x08

/* Timing constants */
#define ENABLE_PULSE_US 500
#define COMMAND_DELAY_US 2000

struct hd44780_ll {
    struct I2cBus *i2c_bus;
    uint8_t i2c_addr;
};

/* Toggle EN (Enable) to latch 4-bit data */
static void hd44780_ll_toggle_enable(struct hd44780_ll *self, uint8_t data)
{
    i2c_write(self->i2c_bus, self->i2c_addr, &data, sizeof(data));

    data |= PIN_EN;
    i2c_write(self->i2c_bus, self->i2c_addr, &data, sizeof(data));
    usleep(ENABLE_PULSE_US);

    data &= ~PIN_EN;
    i2c_write(self->i2c_bus, self->i2c_addr, &data, sizeof(data));
    usleep(ENABLE_PULSE_US);
}

/* Send 4 bits (nibble) to LCD */
static void hd44780_ll_write_nibble(struct hd44780_ll *self, uint8_t nibble, uint8_t mode)
{
    /* Combine 4 data bits (D4–D7) with control bits */
    uint8_t data = (nibble & 0xF0) | mode | BACKLIGHT;
    i2c_write(self->i2c_bus, self->i2c_addr, &data, sizeof(data));
    hd44780_ll_toggle_enable(self, data);
}

/* Send full byte (split into two nibbles) */
static void hd44780_ll_write_byte(struct hd44780_ll *self, uint8_t value, uint8_t mode)
{
    /* High nibble first */
    hd44780_ll_write_nibble(self, value & 0xF0, mode);
    /* Then low nibble */
    hd44780_ll_write_nibble(self, (value << 4) & 0xF0, mode);
    usleep(COMMAND_DELAY_US);
}

/* LCD command */
static void hd44780_ll_command(struct hd44780_ll *self, uint8_t cmd)
{
    hd44780_ll_write_byte(self, cmd, 0x00);
}

/* Initialize LCD in 4-bit mode (datasheet Figure 24) */
struct hd44780_ll *hd44780_ll_create(struct I2cBus *i2c_bus, uint8_t i2c_addr)
{
    struct hd44780_ll *ll = calloc(1, sizeof(struct hd44780_ll));
    if (!ll)
        return NULL;

    ll->i2c_bus = i2c_bus;
    ll->i2c_addr = i2c_addr;

    // usleep(50000); // Wait > 40 ms after power-on

    /* Set 8-bit mode three times (function set) */
    hd44780_ll_write_nibble(ll, 0x30, 0x00);
    usleep(4500);
    hd44780_ll_write_nibble(ll, 0x30, 0x00);
    usleep(150);
    hd44780_ll_write_nibble(ll, 0x30, 0x00);
    usleep(150);

    /* Switch to 4-bit mode */
    hd44780_ll_write_nibble(ll, 0x20, 0x00);
    usleep(150);

    /* Now we can send full commands in 4-bit mode */
    hd44780_ll_command(ll, LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5x8DOTS);
    hd44780_ll_command(ll, LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF);
    hd44780_ll_command(ll, LCD_CLEAR_DISPLAY);

    return ll;
}

/* Set cursor to line (1 or 2) */
void hd44780_ll_set_cursor(struct hd44780_ll *self, uint8_t line)
{
    uint8_t address = (line == 0) ? 0x80 : 0xC0;
    hd44780_ll_command(self, address);
}

/* LCD data (character) */
void hd44780_ll_data(struct hd44780_ll *self, uint8_t data)
{
    hd44780_ll_write_byte(self, data, PIN_RS);
}

/* Clear the hd44780 */
void hd44780_ll_clear(struct hd44780_ll *self)
{
    hd44780_ll_command(self, LCD_CLEAR_DISPLAY);
    /* Clearing the hd44780 takes a bit longer */
    usleep(2000);
}
