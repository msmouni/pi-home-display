#include "ssd1306.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display/oled_128x32/fonts/5x8_vertikal_LSB_1.h"

#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32

#define SSD1306_CHAR_WIDTH 5
#define SSD1306_CHAR_HEIGHT 8
#define SSD1306_SPACING 1
#define SSD1306_CHARS_PER_LINE (SSD1306_WIDTH / (SSD1306_CHAR_WIDTH + SSD1306_SPACING))

#define SSD1306_I2C_CMD 0x00
#define SSD1306_I2C_DATA 0x40

struct ssd1306
{
    struct I2cBus *i2c;
};

static void ssd1306_write_byte(struct ssd1306 *self, uint8_t control, uint8_t data)
{
    uint8_t buf[2] = {control, data};
    i2c_write(self->i2c, SSD1306_I2C_ADDR, buf, 2);
}

static void ssd1306_send_command(struct ssd1306 *self, uint8_t cmd)
{
    ssd1306_write_byte(self, SSD1306_I2C_CMD, cmd); // Co=0, D/C#=0
}

static void ssd1306_send_data(struct ssd1306 *self, uint8_t data)
{
    ssd1306_write_byte(self, SSD1306_I2C_DATA, data); // Co=0, D/C#=1
}

struct ssd1306 *ssd1306_create(struct I2cBus *i2c_bus)
{
    struct ssd1306 *disp = calloc(1, sizeof(struct ssd1306));
    if (!disp)
        return NULL;

    disp->i2c = i2c_bus;
    return disp;
}

int ssd1306_init(struct ssd1306 *self)
{
    // TODO: define command from datasheet (9 COMMAND TABLE)

    ssd1306_send_command(self, 0xAE); // Display OFF
    ssd1306_send_command(self, 0xD5); // Set display clock divide ratio
    ssd1306_send_command(self, 0x80);

    ssd1306_send_command(self, 0xA8); // Set multiplex
    ssd1306_send_command(self, 0x1F); // 0..31 for 128×32

    ssd1306_send_command(self, 0xD3); // Display offset
    ssd1306_send_command(self, 0x00);

    ssd1306_send_command(self, 0x40); // Start line = 0

    ssd1306_send_command(self, 0x8D); // Charge pump
    ssd1306_send_command(self, 0x14); // Enable

    ssd1306_send_command(self, 0x20); // Memory mode
    ssd1306_send_command(self, 0x00); // Horizontal

    ssd1306_send_command(self, 0xA1); // Segment remap
    ssd1306_send_command(self, 0xC8); // COM scan direction

    ssd1306_send_command(self, 0xDA);
    ssd1306_send_command(self, 0x02); // Sequential COM pins

    ssd1306_send_command(self, 0x81);
    ssd1306_send_command(self, 0x7F); // Contrast

    ssd1306_send_command(self, 0xA4); // Resume RAM display
    ssd1306_send_command(self, 0xA6); // Normal display

    ssd1306_clear(self);

    ssd1306_send_command(self, 0xAF); // Display ON

    return 0;
}

void ssd1306_clear(struct ssd1306 *self)
{
    for (uint8_t page = 0; page < 4; page++)
    {
        ssd1306_set_cursor(self, page, 0);
        for (int i = 0; i < 128; i++)
            ssd1306_send_data(self, 0x00);
    }
}

void ssd1306_set_cursor(struct ssd1306 *self, uint8_t page, uint8_t column)
{
    ssd1306_send_command(self, 0xB0 | (page & 0x07));   // Set page
    ssd1306_send_command(self, 0x00 | (column & 0x0F)); // Lower column
    ssd1306_send_command(self, 0x10 | (column >> 4));   // Upper column
}

void ssd1306_draw_char(struct ssd1306 *self, char c)
{
    const char *glyph = font[(uint8_t)c];

    for (int i = 0; i < SSD1306_CHAR_WIDTH; i++)
        ssd1306_send_data(self, glyph[i]); // 5 columns of 8 pixels (bits) per char

    ssd1306_send_data(self, 0x00); // 1px spacing
}

void ssd1306_draw_string(struct ssd1306 *self, uint8_t page, uint8_t col, const char *str)
{
    ssd1306_set_cursor(self, page, col);

    while (*str)
        ssd1306_draw_char(self, *str++);
}

void ssd1306_draw_line(struct ssd1306 *self, uint8_t page, const char *str)
{
    ssd1306_set_cursor(self, page, 0);

    int str_len = strlen(str);

    for (int i = 0; i < SSD1306_CHARS_PER_LINE; i++)
    {
        if (i < str_len)
            ssd1306_draw_char(self, str[i]);
        else
            ssd1306_draw_char(self, ' '); // pad with spaces
    }
}

void ssd1306_clear_line(struct ssd1306 *self, uint8_t page)
{
    ssd1306_set_cursor(self, page, 0);
    for (int i = 0; i < SSD1306_WIDTH; i++)
        ssd1306_send_data(self, 0x00);
}

void ssd1306_destroy(struct ssd1306 *self)
{
    free(self);
}
