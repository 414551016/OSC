
# NYCU OSC Lab1 — RISC-V Bare-Metal Kernel

This project implements **Lab1** for the NYCU Operating Systems Course (OSC).
The goal is to build a minimal **RISC‑V bare‑metal kernel** that runs on:

- **OrangePi RV2 (real hardware)**
- **QEMU RISC‑V virt machine (WSL/Linux)**

The kernel provides:

- UART driver
- Interactive shell
- Command parser
- SBI (Supervisor Binary Interface) system calls

---

# System Architecture

Boot flow on **OrangePi RV2**

SD Card → U‑Boot → kernel.fit → _start → clear_bss() → main() → UART Shell

Boot flow on **WSL / QEMU**

QEMU virt → OpenSBI → kernel (0x80200000) → _start → main() → UART Shell

---

# Project Structure
```text
Lab1/
├── Makefile
├── README.md
├── kernel.its
├── linker_opi.ld
├── linker_qemu.ld
├── x1_orangepi-rv2.dtb
│
├── include
│   ├── sbi.h
│   ├── shell.h
│   ├── string.h
│   └── uart.h
│
└── src
    ├── start.S
    ├── init.c
    ├── main.c
    ├── shell.c
    ├── string.c
    ├── sbi.c
    ├── uart_opi.c
    └── uart_qemu.c
```
---

# Environment

Development environment:

WSL Ubuntu  
riscv64-unknown-elf-gcc  
QEMU  
OpenSBI

Install required tools:

sudo apt update  
sudo apt install gcc-riscv64-unknown-elf qemu-system-misc u-boot-tools

---

# Exercise 2 — UART Setup

Goal:

Implement a **UART driver** to support character input/output.

Key functions:

uart_init()  
uart_putc()  
uart_getc()  
uart_puts()

The UART driver uses **memory‑mapped I/O**.

For OrangePi RV2:

UART Base Address = 0xD4017000

Kernel output example:

Lab1 Exercise2 UART OK

User input is echoed back through UART.

---

# Exercise 3 — Simple Shell

Goal:

Build a minimal interactive shell.

Features:

- prompt display
- command parsing
- line input
- built‑in commands

Supported commands:

help  
hello

Example:

opi-rv2> hello  
Hello World.

---

# Exercise 4 — SBI System Information

Goal:

Use **SBI ecall** to query system information from OpenSBI.

Implemented functions:

sbi_get_spec_version()  
sbi_get_impl_id()  
sbi_get_impl_version()

These functions use:

SBI Base Extension (EID = 0x10)

Example output:

opi-rv2> info

System information
------------------
OpenSBI specification version : 0x0000000001000000
Implementation ID             : 0x0000000000000001
Implementation version        : 0x0000000000010003

---

# Build

Compile the project:

make

Generated files:

kernel.fit   → OrangePi RV2  
kernel.img   → QEMU  
kernel.elf  
qemu.elf

---

# Run on QEMU (WSL)

make run

Expected output:

OpenSBI v1.x
NYCU OSC RISC‑V KERNEL
Lab1 Exercise4 - System Information

Simple shell started.
Type 'help' to list commands.

opi-rv2>

Test commands:

help  
hello  
info

---

# Run on OrangePi RV2

Copy kernel to SD card:

cp kernel.fit /mnt/sd/kernel.fit  
sync

Insert the SD card into OrangePi RV2 and boot.

The shell will appear on the serial console.

---

# Key Implementation Concepts

## Bare‑Metal Programming

The kernel runs without an operating system and directly controls hardware using **memory‑mapped I/O**.

## Device Drivers

UART is implemented using a **polling driver** that waits until hardware registers indicate readiness.

## Supervisor Binary Interface (SBI)

The kernel uses **ecall** to communicate with OpenSBI firmware.

## Cross Compilation

The kernel is compiled using:

riscv64-unknown-elf-gcc

for the RISC‑V architecture.

---

# Learning Experience

This lab helped me understand:

• Boot process: Bootloader → Firmware → Kernel  
• Hardware interaction via MMIO  
• Basic kernel structure and startup assembly  
• Building a minimal shell interface  
• Using SBI calls to obtain system information  
• Differences between QEMU simulation and real hardware

---

# Conclusion

This project successfully implements a minimal RISC‑V kernel with:

- UART driver
- interactive shell
- SBI system calls
- support for **QEMU** and **OrangePi RV2**

It provides a strong foundation for understanding **operating system boot flow, hardware drivers, and low‑level kernel development**.

---

Author: NYCU OSC Lab1

