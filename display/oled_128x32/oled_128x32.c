#include "oled_128x32.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display/oled_128x32/fonts/5x8_vertikal_LSB_1.h"

#define OLED_I2C_CMD 0x00
#define OLED_I2C_DATA 0x40

// TMP: Use a global var for simplicity
static struct I2cBus *g_i2c_bus;

static void oled_128x32_write_byte(uint8_t control, uint8_t data)
{
    uint8_t buf[2] = {control, data};
    i2c_write(g_i2c_bus, OLED_I2C_ADDR, buf, 2);
}

static void oled_128x32_send_command(uint8_t cmd)
{
    oled_128x32_write_byte(OLED_I2C_CMD, cmd); // Co=0, D/C#=0
}

static void oled_128x32_send_data(uint8_t data)
{
    oled_128x32_write_byte(OLED_I2C_DATA, data); // Co=0, D/C#=1
}

int oled_128x32_init(struct I2cBus *i2c_bus)
{
    g_i2c_bus = i2c_bus;

    // TODO: define command from datasheet (9 COMMAND TABLE)

    oled_128x32_send_command(0xAE); // Display OFF
    oled_128x32_send_command(0xD5); // Set display clock divide ratio
    oled_128x32_send_command(0x80);

    oled_128x32_send_command(0xA8); // Set multiplex
    oled_128x32_send_command(0x1F); // 0..31 for 128×32

    oled_128x32_send_command(0xD3); // Display offset
    oled_128x32_send_command(0x00);

    oled_128x32_send_command(0x40); // Start line = 0

    oled_128x32_send_command(0x8D); // Charge pump
    oled_128x32_send_command(0x14); // Enable

    oled_128x32_send_command(0x20); // Memory mode
    oled_128x32_send_command(0x00); // Horizontal

    oled_128x32_send_command(0xA1); // Segment remap
    oled_128x32_send_command(0xC8); // COM scan direction

    oled_128x32_send_command(0xDA);
    oled_128x32_send_command(0x02); // Sequential COM pins

    oled_128x32_send_command(0x81);
    oled_128x32_send_command(0x7F); // Contrast

    oled_128x32_send_command(0xA4); // Resume RAM display
    oled_128x32_send_command(0xA6); // Normal display

    oled_128x32_clear();

    oled_128x32_send_command(0xAF); // Display ON

    return 0;
}

void oled_128x32_clear(void)
{
    for (uint8_t page = 0; page < 4; page++)
    {
        oled_128x32_set_cursor(page, 0);
        for (int i = 0; i < 128; i++)
            oled_128x32_send_data(0x00);
    }
}

void oled_128x32_set_cursor(uint8_t page, uint8_t column)
{
    oled_128x32_send_command(0xB0 | (page & 0x07));   // Set page
    oled_128x32_send_command(0x00 | (column & 0x0F)); // Lower column
    oled_128x32_send_command(0x10 | (column >> 4));   // Upper column
}

void oled_128x32_draw_char(char c)
{
    const char *glyph = font[(uint8_t)c];

    for (int i = 0; i < 5; i++)
        oled_128x32_send_data(glyph[i]); // 5 columns of 8 pixels (bits) per char

    oled_128x32_send_data(0x00); // 1px spacing
}

void oled_128x32_draw_string(uint8_t page, uint8_t col, const char *str)
{
    oled_128x32_set_cursor(page, col);

    while (*str)
        oled_128x32_draw_char(*str++);
}

void oled_128x32_clear_line(uint8_t page)
{
    oled_128x32_set_cursor(page, 0);
    for (int i = 0; i < OLED_WIDTH; i++)
        oled_128x32_send_data(0x00);
}

void oled_128x32_close()
{
    // TODO: complete if needed
}
