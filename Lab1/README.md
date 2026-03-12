
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
├── Makefile             # 編譯與打包
├── README.md
├── kernel.its
├── linker_opi.ld        # 適用於 Orange Pi RV2 記憶體配置
├── linker_qemu.ld       # 適用於 WSL OpenSBI 記憶體配置
├── x1_orangepi-rv2.dtb
│
├── include
│   ├── sbi.h            # SBI 宣告與常數
│   ├── shell.h
│   ├── string.h
│   └── uart.h
│
└── src
    ├── start.S           # 自己寫簡單字串函式
    ├── init.c            # 清除 .bss
    ├── main.c            # 初始化 UART，進入 shell
    ├── shell.c           # Exercise 3 的 shell 邏輯
    ├── string.c          # kernel 入口，設定 stack，清 .bss，進入 main
    ├── sbi.c             # Exercise 4 核心，實作 sbi_ecall
    ├── uart_opi.c        # Exercise 2 的 UART driver
    └── uart_qemu.c       # Exercise 2 的 UART driver
```
- 把 QEMU 與 OrangePi 的 UART driver 分開做法，因為兩者 MMIO base 與存取寬度不同。
- 沒有依賴標準函式庫，而是自己實作 strcmp、strlen、整數輸出、十六進位輸出，這很符合 bare-metal kernel 的訓練目標。
- uart_init() 保持空實作，原因不是漏寫，而是因為目前執行環境已先把 UART 初始化完成。這表示你有分清楚「哪些事情這個 lab 要自己做、哪些事情是 firmware / QEMU 已幫你做」。

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

# Exercise 1 — 指令用途逐一說明
start.S：
- .section .text：把後面的內容放到程式碼區段。
- .global _start：把 _start 匯出成全域符號，讓 linker 能把它當入口。
- .extern main / clear_bss / _stack_top：宣告這些符號在別處定義。
- la sp, _stack_top：把 _stack_top 位址載入 sp，建立堆疊。
- call clear_bss：呼叫清 BSS 函式。
- call main：進入 C 主程式。
- 1: j 1b：跳回前面的標號 1，形成無限迴圈。1b 的意思是「往回找最近的標號 1」。

init.c：
- extern char _bss_start[]; extern char _bss_end[];：引用 linker 產生的區段邊界。
- char *p = _bss_start;：用指標從 BSS 起點開始掃。
- while (p < _bss_end)：只要還沒到 BSS 結尾就持續清。
- *p++ = 0;：把目前位址設 0，再把指標往後移一格。

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


# Exercise 2 內容重點
- uart_init() 目前是空的，因為你的註解寫得很清楚：OrangePi 版是 firmware 先初始化 UART，QEMU 版則是 - QEMU + OpenSBI 先初始化，所以你的 kernel 先直接接手使用。
- uart_putc() 採 polling：一直讀 line status register，等到 transmit holding register empty (UART_LSR_THRE) 才送出字元。
- uart_getc() 也是 polling：等到 data ready (UART_LSR_DR) 才讀取接收字元。
- uart_puts() 就是反覆呼叫 uart_putc()，把字串一個字一個字送出去。

# Exercise 2 指令 / 程式用途逐一說明
main.c：
- #include "shell.h"：引用 shell 介面。
- #include "uart.h"：引用 UART 介面。
- uart_init();：初始化或接手 UART。
- uart_puts("...")：印出啟動訊息。
- shell_run();：把控制權交給 shell 主迴圈。

uart_opi.c：
- #define UART_BASE 0xD4017000UL：OrangePi RV2 UART 基底位址。
- #define REG_SHIFT 2：OrangePi 這版寄存器位址需要左移 2，相當於每個 register 間隔 4 bytes。
- UART_RBR / UART_THR / UART_LSR：分別是接收、傳送、狀態寄存器索引。
- UART_LSR_DR 0x01：資料已到。
- UART_LSR_THRE 0x20：傳送暫存器已空，可以送下一個字。
- reg(r)：把 register 編號換算成實際 MMIO 位址。
- mmio_read()：從硬體位址讀 32-bit 值。
- mmio_write()：把 32-bit 值寫到硬體位址。
- if (c == '\n') uart_putc('\r');：自動把 LF 轉成 CR+LF，讓終端顯示換行正常。
- while (!(... & UART_LSR_THRE)) {}：忙等到硬體可寫。
- mmio_write(reg(UART_THR), ...)：真正把字元送進 UART。
- while (!(... & UART_LSR_DR)) {}：忙等到硬體收到資料。
- return (char)(mmio_read(...) & 0xff);：讀出最低 8 bits 作為字元。

uart_qemu.c：
- 結構跟 OrangePi 版幾乎一樣，但基底位址是 0x10000000UL，存取寬度是 8-bit，因此用 mmio_read8/mmio_write8。


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


# Exercise 3 內容重點
- 顯示 shell 提示字元 print_prompt() 固定印 opi-rv2> ，讓使用者知道可以輸入命令。
- read_line() 逐字讀入，遇到 Enter 結束，遇到 Backspace 會刪掉前一字，最後補上 '\0' 形成 C 字串。
- execute_command() 用 strcmp() 比對命令字串。符合 help、hello、info 就執行對應行為；空字串忽略；其他則回報 unknown command。
- 因為你自己實作了 strcmp() 和 strlen()，所以 kernel 不依賴標準 C library。這很符合 bare-metal 環境。

# Exercise 3 指令 / 程式用途逐一說明
shell.c：
- #define CMD_BUF_SIZE 64：命令緩衝區最大長度 64 bytes。
- static void print_prompt(void)：顯示 shell 提示字元。
- static void read_line(char *buf, int max_len)：從 UART 讀一整行命令。
- char c = uart_getc();：讀一個字元。
- if (c == '\r' || c == '\n')：碰到 Enter，結束輸入。
- uart_putc('\n');：輸入完幫你換到下一行。
- if (c == '\b' || c == 127)：支援 Backspace / Delete。
- uart_puts("\b \b");：終端機上把前一個字 visually 擦掉。
- if (i < max_len - 1)：保留最後一格給字串結尾 '\0'。
- buf[i] = '\0';：把讀到的內容變成標準 C 字串。

string.c：
- strcmp()：逐字比較兩字串；若完全相同回傳 0，不同則回傳第一個不同字元的差值。
- strlen()：從頭數到 '\0' 為止，回傳長度。
- execute_command()：
- strcmp(cmd, "help") == 0：判斷是不是 help。
- strcmp(cmd, "hello") == 0：判斷是不是 hello。
- strcmp(cmd, "info") == 0：判斷是不是 info。
- strlen(cmd) == 0：空白輸入直接忽略。
- else { Unknown command... }：處理非法指令。


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
```text
OpenSBI specification version : 0x0000000001000000
Implementation ID             : 0x0000000000000001
Implementation version        : 0x0000000000010003
```

# Exercise 4 內容重點
- struct sbiret 有兩個欄位：error 和 value，對應 SBI ecall 的回傳結果。
- sbi_ecall() 把參數放進 RISC-V 慣例的 a0 到 a7 暫存器，然後執行 ecall。呼叫後，a0 代表錯誤碼，a1 代表回傳值。
- SBI_EXT_BASE 是 0x10，而 GET_SPEC_VERSION、GET_IMPL_ID、GET_IMPL_VERSION 分別是 function ID 0/1/2。
- print_info() 裡除了讀 SBI，還自己做了十進位與十六進位輸出工具 uart_put_uint() 與 uart_put_hex()，所以不需要 printf()。

# Exercise 4 指令 / 程式用途逐一說明
sbi.h：
- struct sbiret { long error; long value; };：封裝 SBI 回傳結果。
- #define SBI_EXT_BASE 0x10：Base Extension 的 extension ID。
- #define SBI_EXT_BASE_GET_SPEC_VERSION 0：查 SBI 規格版本。
- #define SBI_EXT_BASE_GET_IMPL_ID 1：查實作 ID。
- #define SBI_EXT_BASE_GET_IMPL_VERSION 2：查實作版本。

sbi.c：
- register unsigned long a0 asm("a0") = arg0; 到 a7：把 C 變數綁定到指定 RISC-V 暫存器。
- a6 = fid; a7 = ext;：依 SBI 呼叫慣例，a7 放 extension ID，a6 放 function ID。
- asm volatile("ecall" ... )：發出環境呼叫，交給 OpenSBI 處理。
- "+r"(a0), "+r"(a1)：表示 a0/a1 既是輸入也是輸出。
- "memory" clobber：告訴編譯器這段 inline asm 可能影響記憶體，不要亂重排。
- ret.error = (long)a0; ret.value = (long)a1;：收回 SBI 回傳值。
- sbi_get_spec_version() / sbi_get_impl_id() / sbi_get_impl_version()：都是把對應的 fid 丟進 sbi_ecall() 後取 ret.value。

shell.c 裡與 Exercise 4 有關的部分：
- uart_put_uint()：把無號整數轉成十進位字串輸出。做法是先反向取餘數存進 buffer，再倒著印。
- uart_put_hex()：固定以 0x 開頭，逐 nibble 取值輸出十六進位。
- unsigned long major = (spec >> 24) & 0x7f;：從 spec 取 major 版本。
- unsigned long minor = spec & 0xffffff;：從 spec 取 minor 版本。
- print_info()：整合顯示版本與實作資訊。




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

在 RISC-V 系統中，作業系統不能直接做某些硬體相關操作，因此需要透過 SBI (Supervisor Binary Interface) 呼叫 firmware 服務，而 sbi.c 就是負責實作這個呼叫機制。在 shell 中輸入 info 時，就是透過 sbi.c 向 OpenSBI 查詢系統資訊。
```text
sbi_ecall()
sbi_get_spec_version()
sbi_get_impl_id()
sbi_get_impl_version()
| 函式                   | 用途                  |
| -------------------- | ------------------- |
| sbi_ecall            | 真正執行 RISC-V `ecall` |
| sbi_get_spec_version | 取得 OpenSBI 規格版本     |
| sbi_get_impl_id      | 取得實作 ID             |
| sbi_get_impl_version | 取得實作版本              |

```

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

# Lab1 演進過程
- Exercise 1 建立最小執行環境：_start → stack → clear_bss → main。
- Exercise 2 補上輸入輸出能力：main → uart_init → uart_puts / uart_getc，讓 kernel 能看到訊息、能接受鍵盤輸入。
- Exercise 3 把 I/O 包成互動介面：shell_run → read_line → execute_command，讓使用者能透過 help/hello 控制 kernel。
- Exercise 4 再把 shell 接上 SBI：info → sbi_get_* → ecall → OpenSBI，讓 kernel 開始能「詢問系統」。
- 所以最後整體流程其實就是：
Boot → _start → clear_bss → main → uart_init → shell_run → 使用者輸入 → execute_command → (必要時) sbi_ecall。

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


