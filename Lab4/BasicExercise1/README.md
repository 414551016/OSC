執行結果：make run
```
lss@DESKTOP-2MCFI1H:~/OscLab4/BasicExercise1$ make run
rm -f kernel kernel.elf *.o
riscv64-unknown-elf-gcc -mcmodel=medany -ffreestanding -nostdlib -g -Wall -c *.S *.c
riscv64-unknown-elf-ld -T link.ld -o kernel.elf *.o
riscv64-unknown-elf-objcopy -O binary kernel.elf kernel
qemu-system-riscv64 -M virt -m 8G -kernel kernel -display none -serial stdio -initrd initramfs.cpio

OpenSBI v0.9
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name             : riscv-virtio,qemu
Platform Features         : timer,mfdeleg
Platform HART Count       : 1
Firmware Base             : 0x80000000
Firmware Size             : 100 KB
Runtime SBI Version       : 0.2

Domain0 Name              : root
Domain0 Boot HART         : 0
Domain0 HARTs             : 0*
Domain0 Region00          : 0x0000000080000000-0x000000008001ffff ()
Domain0 Region01          : 0x0000000000000000-0xffffffffffffffff (R,W,X)
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Arg1         : 0x00000000bf000000
Domain0 Next Mode         : S-mode
Domain0 SysReset          : yes

Boot HART ID              : 0
Boot HART Domain          : root
Boot HART ISA             : rv64imafdcsu
Boot HART Features        : scounteren,mcounteren,time
Boot HART PMP Count       : 16
Boot HART PMP Granularity : 4
Boot HART PMP Address Bits: 54
Boot HART MHPM Count      : 0
Boot HART MHPM Count      : 0
Boot HART MIDELEG         : 0x0000000000000222
Boot HART MEDELEG         : 0x000000000000b109

Starting kernel ...
=== S-Mode trap ===
scause: 0x0000000000000008
sepc: 0x0000000080205002
stval: 0x0000000000000000
=== S-Mode trap ===
scause: 0x0000000000000008
sepc: 0x0000000080205002
stval: 0x0000000000000000
=== S-Mode trap ===
scause: 0x0000000000000008
sepc: 0x0000000080205002
stval: 0x0000000000000000
=== S-Mode trap ===
scause: 0x0000000000000008
sepc: 0x0000000080205002
stval: 0x0000000000000000
=== S-Mode trap ===
scause: 0x0000000000000008
sepc: 0x0000000080205002
stval: 0x0000000000000000

```

## Basic Exercise 1 作業目標 + TODO 對照表
| 作業目標                               | 對應 TODO                                          | 你要完成的內容                                                                                                   | 對應檔案      |
| ---------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------------------------------- | --------- |
| 從 S-mode 切到 U-mode 執行 user program | Finish `exec` in `main.c`                        | 從 initramfs 找到 `prog.bin`、解析 CPIO header、複製 user program、設定 `sepc`/`sstatus`/`sscratch`，再 `sret` 進 U-mode | `main.c`  |
| 設定 trap 入口                         | Understand `handle_exception` in `Start.S`       | 設 `stvec` 指向 trap entry；trap 發生時先存所有暫存器與 `sepc/sstatus/scause/stval`                                      | `start.S` |
| 定義 trap frame                      | Define the trap frame                            | `struct pt_regs` 欄位順序必須和 `start.S` 的 save/restore 順序完全一致                                                  | `main.c`  |
| 實作 C-level exception handler       | implement `do_trap` to print `sepc` and `scause` | 印出 `sepc`、`scause`、`stval`；對 `scause == 8` 做 `sepc += 4`，避免無限重複執行 `ecall`                                 | `main.c`  |
| 回到 U-mode 繼續執行                     | trap handling flow                               | `do_trap()` 返回後，由 `start.S` 還原 context，最後 `sret` 返回 user program                                          | `start.S` |


