CROSS   = riscv64-unknown-elf-
CC      = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

CFLAGS_COMMON = -Wall -O2 -ffreestanding -nostdlib -Iinclude
CFLAGS_OPI    = $(CFLAGS_COMMON)
CFLAGS_QEMU   = $(CFLAGS_COMMON) -mcmodel=medany

COMMON_SRC = src/start.S src/init.c src/main.c src/string.c src/shell.c src/sbi.c
OPI_SRC    = $(COMMON_SRC) src/uart_opi.c
QEMU_SRC   = $(COMMON_SRC) src/uart_qemu.c

SD ?= /mnt/sd

all: kernel.fit kernel.img

# ----------------------------
# OrangePi RV2 build
# ----------------------------
kernel.elf: $(OPI_SRC) linker_opi.ld
	$(CC) $(CFLAGS_OPI) -T linker_opi.ld -o kernel.elf $(OPI_SRC)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

kernel.fit: kernel.bin kernel.its x1_orangepi-rv2.dtb
	mkimage -f kernel.its kernel.fit

# ----------------------------
# QEMU / WSL build
# ----------------------------
qemu.elf: $(QEMU_SRC) linker_qemu.ld
	$(CC) $(CFLAGS_QEMU) -T linker_qemu.ld -o qemu.elf $(QEMU_SRC)

kernel.img: qemu.elf
	$(OBJCOPY) -O binary qemu.elf kernel.img

run: qemu.elf
	qemu-system-riscv64 -M virt -nographic -bios default -kernel qemu.elf

run-img: kernel.img
	qemu-system-riscv64 -M virt -nographic -bios default -kernel kernel.img

copy: kernel.fit
	cp kernel.fit $(SD)/kernel.fit
	sync

clean:
	rm -f kernel.elf kernel.bin kernel.fit qemu.elf kernel.img
