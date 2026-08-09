# Engine info
NAME = Magpie
VERSION = dev

BUILD_DIR = build

# Target binaries
TARGET = $(BUILD_DIR)/$(NAME)_$(VERSION)
TUNE_TARGET = $(BUILD_DIR)/$(NAME)_tune
WIN_TARGET = $(BUILD_DIR)/$(NAME)_$(VERSION).exe

# Compiler and flags
CC = gcc
WIN_CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -Ofast -march=native -DENGINE_VERSION=\"$(VERSION)\" -DENGINE_NAME=\"$(NAME)\"
TUNE_CFLAGS = $(CFLAGS) -DTUNE_BUILD

# Header dependency tracking
HEADERS = magpie.h

# Source files
SRCS = main.c board.c movegen.c play.c uci.c terminal.c
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
TUNE_OBJS = $(addprefix $(BUILD_DIR)/tune_, $(SRCS:.c=.o))

all: $(TARGET) $(TUNE_TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

tune: $(TUNE_TARGET)

$(TUNE_TARGET): $(TUNE_OBJS) | $(BUILD_DIR)
	$(CC) $(TUNE_CFLAGS) -o $@ $(TUNE_OBJS)

windows: $(WIN_TARGET)

$(WIN_TARGET): $(SRCS) $(HEADERS) | $(BUILD_DIR)
	$(WIN_CC) $(CFLAGS) -o $@ $(SRCS)

$(BUILD_DIR)/tune_%.o: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(TUNE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all tune windows clean

