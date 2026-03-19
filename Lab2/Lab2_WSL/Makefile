CROSS ?= riscv64-unknown-elf
CC      = $(CROSS)-gcc
OBJCOPY = $(CROSS)-objcopy

CFLAGS  = -Wall -Wextra -O0 -g \
          -ffreestanding -nostdlib -nostartfiles \
          -march=rv64gc -mabi=lp64 \
          -mcmodel=medany -fno-pie

LDFLAGS = -T linker.ld -nostdlib -no-pie

all: bootloader.bin

bootloader.elf: start.S bootloader.c linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) start.S bootloader.c -o $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootloader.elf bootloader.bin