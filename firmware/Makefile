CC = avr-gcc
CFLAGS = -g -O1 -mmcu=atmega8a -flto
OBJCOPY = avr-objcopy
SIZE = avr-size
LDFLAGS = -flto -Wl,--print-memory-usage

SRC_DIR = src
BUILD_DIR = build
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(BUILD_DIR)/main.hex size

$(BUILD_DIR)/main.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/main.hex: $(BUILD_DIR)/main.elf
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@

size: $(BUILD_DIR)/main.elf
	$(SIZE) -C --mcu=atmega8a $^
	@echo ""
	@echo "Note: Stack usage analysis disabled with LTO (remove -flto to see stack details)"

clean:
	-rm -rf $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

.PHONY: all size clean
