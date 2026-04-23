CROSS_COMPILE ?= riscv64-unknown-elf-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

CFLAGS  := -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles \
           -march=rv64gc -mabi=lp64 -mcmodel=medany -Iinclude
ASFLAGS := $(CFLAGS)
LDFLAGS := -T linker.ld

OBJS := start.o kernel/main.o kernel/printk.o kernel/dtb.o kernel/mm.o

all: kernel.bin kernel.fit

start.o: start.S
	$(CC) $(ASFLAGS) -c $< -o $@

kernel/main.o: kernel/main.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/printk.o: kernel/printk.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/dtb.o: kernel/dtb.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/mm.o: kernel/mm.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

kernel.dis: kernel.elf
	$(OBJDUMP) -D $< > $@

kernel.fit: kernel.its kernel.bin x1_orangepi-rv2.dtb
	mkimage -f kernel.its kernel.fit

clean:
	rm -f start.o kernel/*.o kernel.elf kernel.bin kernel.fit kernel.itb kernel.dis

.PHONY: all clean