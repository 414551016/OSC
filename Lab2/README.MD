# 產生 Orange Pi RV2 用的 kernel.fit
```
make clean
make BOARD=rv2
make BOARD=rv2 kernel.fit
```

# Interact with OrangePi RV2

</>Windows PowerShell
```
usbipd list
usbipd bind --busid 5-1
usbipd attach --wsl --busid 5-1
usbipd detach --busid 5-1
usbipd unbind --busid 5-3
```
</>Windows Subsystem for Linux
```
ls -al kernel.*
sudo mkdir -p /mnt/sdcard
sudo mount -t ext4 /dev/sde1 /mnt/sdcard
sudo rm -f /mnt/sdcard/kernel.fit
sudo cp kernel.fit /mnt/sdcard/
ls -al /mnt/sdcard
sync
sudo umount /mnt/sdcard/
```
```
Windows Termainal(1)
sudo screen /dev/ttyUSB0 115200
```

# Basic Exercise 1 - UART Bootloader
演示重點：

1.  Bootloader 可以正常啟動（看到 boot>）
2.  可以透過 UART 接收 kernel
3.  可以跳到 kernel 執行
## Step 1-1：啟動 Orange Pi RV2 開機
```
Starting kernel ...
    
== OSC Lab2 Bootloader ID：414551016 ==
commands:
    help      - show help
    dtb       - show dtb/initrd info
    load      - load kernel over uart
    boot      - jump to loaded kernel
    ls        - list initramfs files
    cat FILE  - show file content from initramfs
boot>
```
- ✔ FIT boot 成功
- ✔ bootloader 有跑起來

## Step 1-2：展示基本功能
```
boot> help
boot> dtb
boot> ls
boot> cat hello.txt
boot> cat readme.txt
```

## Step 1-3：進入 UART loading 模式
```
</>WSL Termainal(1)
boot> load

- 畫面會停在：
waiting BOOT header...
loading kernel size=0x0000000000001731
kernel loaded at 0x0000000020000000
```

## Step 1-4：從 WSL 送出 kernel
```
</>WSL Termainal(2)
sudo python3 send_kernel.py /dev/ttyUSB0 bootloader.bin

- 成功時 WSL(1)會在 OrangePI RV2 顯示
[sudo] password for lss:
sent 5937 bytes to /dev/ttyUSB0
```

## Step 1-5：執行 kernel
```
</>WSL Termainal(1)
boot> boot

- 畫面會顯示
jumping to kernel...

== OSC Lab2 Bootloader ID：414551016 ==
commands:
  help      - show help
  dtb       - show dtb/initrd info
  load      - load kernel over uart
  boot      - jump to loaded kernel
  ls        - list initramfs files
  cat FILE  - show file content from initramfs
```


# Basic Exercise 2 - Devicetree

## Step 2-1：開機進 bootloader
```
== Lab2 Bootloader ==
boot>
```

## Step 2-2：輸入 dtb 指令
```
boot> dtb

輸出
hartid=0x0000000000000000
dtb=0x000000007dd70000
uart_base=0x00000000d4017000
initrd_start=0x000000007dd8f000
initrd_end=0x000000007dd8f400
load_addr=0x0000000020000000
```
我從 bootloader entry 取得 a0 和 a1，a0 是 hartid，a1 是 device tree pointer
parsing FDT 結構：
- "/chosen/linux,initrd-start"
- "/chosen/linux,initrd-end"
- "/soc/serial 的 reg"

來取得：
- initramfs 記憶體位置
- UART base address
- Q1：怎麼 parse DTB？
  <br>走 FDT structure block，解析 token：FDT_BEGIN_NODE, FDT_PROP 再透過 string block 找 property name
- Q2：為什麼要 parse /chosen？
  <br>因為 kernel / bootloader 會把 initrd 的位置放在 /chosen
- Q3：UART base 怎麼來？
  <br>從 /soc/serial 的 reg property 解析
- Q4：為什麼不用 hardcode？
  <br>因為不同平台 UART base 不同，用 DTB 可以做到 platform independent


# Basic Exercise 3 - Initial Ramdisk
- 教學資源：[New ASCII 格式 Cpio 檔案的結構](https://man.freebsd.org/cgi/man.cgi?query=cpio&sektion=5)
```
sudo apt install cpio
cd rootfs
find . | cpio -o -H newc > ../initramfs.cpio
cd ..

檢查 cpio：
cpio -it < ../initramfs.cpio

重新產生 FIT
mkimage -f kernel.its kernel.fit

複製到 SD 卡
cp kernel.fit /media/.../
sync
```
- 使用 New ASCII 格式 Cpio 格式來建立 cpio 壓縮檔。先建立一個 rootfs 目錄，把你需要的所有檔案放進去。然後，請使用上述指令來歸檔。
- Q1：initramfs 在哪？
  <br>initrd_start ~ initrd_end
  <br>boot> dtb
- Q2：initramfs 是什麼格式？
  <br>cpio (newc format)
- Q3：parse cpio？
  <br>每個檔案都有 header（110 bytes）裡面包含：filename length, file size 然後依序走 memory parsing
- Q4：ls 怎麼做？
  <br>逐個解析 cpio entry，印出 filename
- Q5：cat 怎麼做？
  <br>找到指定 filename 然後把 data 印出來


# Advanced Exercise
```
透過 UART 載入 kernel（不靠 SD 卡），並執行它流程：
Step 1：進 bootloader
boot>

Step 2：WSL(1) 輸入 load（關鍵指令）
boot> load

Step 3：在 WSL(2) 端送 kernel
sudo python3 send_kernel.py /dev/ttyUSB0 kernel.bin 或 sudo python3 send_kernel.py /dev/ttyUSB0 kernel.img

Step 4：boot
boot> boot
```
- Q1：kernel 載到哪？
  <br>load_addr（例如 0x20000000）這個地址也可以從 DTB 或 bootloader 設定取得。
- Q2：為什麼不用 SD card？
  <br>UART boot 可以在沒有 storage 的情況下開機，適合 debug 或 early bring-up
- Q3：load 做了什麼？
  <br>透過 UART 逐 byte 接收資料存到指定 memory address
- Q4：boot 做了什麼？
  <br>設定 PC = load address 跳轉到 kernel entry point


# OSC2026 Lab2 -- Booting (UART Bootloader)

NYCU Operating System Capstone Lab\
Lab2: Booting

This project implements a **RISC‑V UART bootloader** that runs on both
**QEMU** and **Orange Pi RV2**.

The bootloader supports: - Device Tree (DTB) parsing - initramfs (CPIO
newc) parsing - UART communication - interactive shell commands -
loading a kernel via UART - jumping to the loaded kernel

------------------------------------------------------------------------

# 1. Lab2 Objectives

Goals of this lab:

1.  Understand the **RISC‑V boot flow**
2.  Implement a **minimal bootloader**
3.  Parse **Device Tree Blob (DTB)**
4.  Parse **initramfs (cpio newc format)**
5.  Implement **UART communication**
6.  Load kernel image through **UART**
7.  Boot the loaded kernel

Bootloader shell commands:

    help
    dtb
    ls
    cat
    load
    boot

------------------------------------------------------------------------

# 2. Boot Flow

System boot flow:
    
    Boot ROM
       ↓
    OpenSBI / U‑Boot
       ↓
    kernel.fit
       ↓
    Lab2 Bootloader
       ↓
    UART load kernel
       ↓
    Jump to kernel entry

Memory layout:

  Component             Address
  --------------------- ------------
  Bootloader            0x00200000
  Kernel load address   0x20000000
  Ramdisk               0x46100000

------------------------------------------------------------------------

# 3. Project Structure

    Lab2/
    ├── bootloader.c
    ├── start.S
    ├── linker.ld
    ├── Makefile
    ├── kernel.its
    ├── send_kernel.py
    ├── initramfs.cpio
    ├── x1_orangepi-rv2.dtb
    └── rootfs/
        ├── hello.txt
        └── readme.txt

------------------------------------------------------------------------

# 4. Environment Setup

Install required tools:

``` bash
sudo apt update

sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf device-tree-compiler u-boot-tools cpio picocom
```

------------------------------------------------------------------------

# 5. Build initramfs

    mkdir rootfs

    echo "Hello from initramfs" > rootfs/hello.txt
    echo "This is lab2 cpio test file." > rootfs/readme.txt

Create archive:

    cd rootfs
    find . | cpio -o -H newc > ../initramfs.cpio
    cd ..

------------------------------------------------------------------------

# 6. Build Bootloader

    make clean
    make BOARD=rv2

Output:

    bootloader.elf
    bootloader.bin

------------------------------------------------------------------------

# 7. Build kernel.fit

    make BOARD=rv2 kernel.fit

Check:

    mkimage -l kernel.fit

------------------------------------------------------------------------

# 8. QEMU Verification

    qemu-system-riscv64 -machine virt -m 512M -nographic -bios default -kernel bootloader.bin -initrd initramfs.cpio -serial mon:stdio

Expected output:

    == Lab2 Bootloader ==
    boot>

Test:

    boot> dtb
    boot> ls
    boot> cat hello.txt

------------------------------------------------------------------------

# 9. UART Kernel Loading

    boot> load

Host:

    python3 send_kernel.py /dev/ttyUSB0 kernel.bin

Then:

    boot> boot

------------------------------------------------------------------------

# 10. Deploy to Orange Pi RV2

Copy FIT:

    cp kernel.fit /boot/
    sync

Boot via U‑Boot:

    fatload mmc 0:1 ${kernel_addr_r} kernel.fit
    bootm ${kernel_addr_r}

------------------------------------------------------------------------

# 11. Serial Console

    picocom -b 115200 /dev/ttyUSB0

Expected:

    == Lab2 Bootloader ==
    boot>

------------------------------------------------------------------------

# 12. Bootloader Commands

  Command   Description
  --------- ----------------------
  help      show help
  dtb       display DTB info
  ls        list initramfs files
  cat       show file content
  load      load kernel via UART
  boot      jump to kernel

------------------------------------------------------------------------

# 13. Lab2 Achievements

Completed:

-   UART bootloader
-   DTB parsing
-   initramfs parsing
-   interactive shell
-   UART kernel loading
-   kernel execution

Verified on:

-   QEMU
-   Orange Pi RV2

------------------------------------------------------------------------

# References

Linux RISC‑V Boot Requirements\
https://docs.kernel.org/arch/riscv/boot.html

Device Tree Specification\
https://github.com/devicetree-org/devicetree-specification

NYCU OSC2026 Lab2\
https://nycu-caslab.github.io/OSC2026/labs/lab2.html

------------------------------------------------------------------------

# 解釋名詞

## 什麼是 UART Bootloader？
UART bootloader 是嵌入式系統中一段燒錄在晶片內部（ROM 或 Flash 開頭）的小型程式，它的功能是：透過 UART（串列埠）來接收外部提供的韌體，並將韌體寫入微控制器的 Flash。

簡單來說：在沒有 USB 、 SD 卡 、 JTAG 環境下，也能靠 UART（TX/RX）下載你的程式到 MCU（或 SBC），因此，UART bootloader 是「最簡單、最普遍、最相容的燒錄方法」。


