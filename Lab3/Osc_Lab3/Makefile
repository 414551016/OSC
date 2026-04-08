# =========================================
# Orange Pi RV2 / RISC-V Lab3 minimal kernel
# 在 WSL 上使用 riscv64-unknown-elf-gcc 編譯
# =========================================

CROSS_COMPILE ?= riscv64-unknown-elf-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

CFLAGS := -Wall -Wextra -O0 -g \
          -ffreestanding -nostdlib -nostartfiles \
          -march=rv64gc -mabi=lp64d -mcmodel=medany \
          -Iinclude

LDFLAGS := -T linker.ld -nostdlib

SRC_C := src/main.c src/uart.c src/printk.c src/startup_alloc.c
SRC_S := src/start.S
OBJ   := $(SRC_C:.c=.o) $(SRC_S:.S=.o)

TARGET_ELF := kernel.elf
TARGET_BIN := kernel.bin
TARGET_DIS := kernel.dis

all: $(TARGET_ELF) $(TARGET_BIN) $(TARGET_DIS)

$(TARGET_ELF): $(OBJ) linker.ld
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

$(TARGET_DIS): $(TARGET_ELF)
	$(OBJDUMP) -D $< > $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET_ELF) $(TARGET_BIN) $(TARGET_DIS)

.PHONY: all clean