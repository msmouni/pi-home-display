# =========================================================
# Toolchain
# =========================================================
CC ?= aarch64-linux-gnu-gcc

# =========================================================
# Project
# =========================================================
TARGET := pi-home-display

BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

# =========================================================
# Source directories
# =========================================================
SRC_DIRS := \
    . \
    pi-home-utils/i2c \
    pi-home-utils/db \
    display \
    display/lcd_16x2/hd44780 \
    display/lcd_16x2/hd44780/low_level \
    display/oled_128x32 \
    display/oled_128x32/ssd1306

# =========================================================
# Compiler / Linker flags
# =========================================================
CFLAGS := -Wall -Wextra -O2
CFLAGS += $(addprefix -I,$(SRC_DIRS))

LDFLAGS := -lsqlite3

# =========================================================
# Source and Object Files
# =========================================================
SRCS := \
    main.c \
    pi-home-utils/i2c/i2c.c \
    pi-home-utils/db/db.c \
    display/display.c \
    display/lcd_16x2/hd44780/hd44780.c \
    display/lcd_16x2/hd44780/low_level/low_level.c\
    display/lcd_16x2/lcd_16x2_display.c \
    display/oled_128x32/oled_display.c \
    display/oled_128x32/ssd1306/ssd1306.c

OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)

# =========================================================
# Default target
# =========================================================
all: $(BIN_DIR)/$(TARGET)

# =========================================================
# Link
# =========================================================
$(BIN_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# =========================================================
# Compile
# =========================================================
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# =========================================================
# Clean
# =========================================================
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
