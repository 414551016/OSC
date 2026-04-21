Q1：
https://github.com/414551016/Operating-System-Capstone/blob/main/Lab4/Lab4.md 
https://github.com/riscv/riscv-isa-manual/releases 
1.請詳閱讀內容及參考檔案，請說明Lab4作業須要完成任務目標。 
2.我是初學者角色學習，請對以上「實作與寫作業的完整路線圖」步驟說明我該如何。

我先把重點講清楚：你這份 Lab4 不只是做「例外與中斷」的概念驗證，而是要把你目前的小型核心，補上「U-mode 執行、trap 進 S-mode、timer interrupt、PLIC+UART interrupt」這四條主線。 投影片版重點放在前兩個 in-class exercises；GitHub 的 Lab4.md 則再往下延伸，包含 core timer 與 advanced timer multiplexing。
```
1. Lab4 作業需要完成的任務目標
A. 先看投影片版：你課堂上至少要完成的兩個 exercise

投影片第 2 頁明確寫出這次 lab 有兩個 in-class exercises：

Exercise 4-1: Privilege Level Switching and Exception Handling
Exercise 4-2: PLIC and UART Interrupts。
Exercise 4-1 的任務

投影片第 3～7 頁說明，這部分核心目標是：

讓 kernel 在 S-mode
切到 user program 在 U-mode 執行
當 user program 發生 exception / ecall 時，再 trap 回 S-mode
在 trap handler 中讀出並處理相關 CSR。

投影片直接列出的 TODO 是：

了解 handle_exception in Start.S
定義 trap frame，並實作 do_trap，印出 sepc 與 scause
完成 main.c 裡的 exec。

投影片也列出你必須掌握的 S-mode CSR：

sstatus：其中 SPP 要設成能返回 U-mode 的狀態
sepc：trap return address
scause：trap 原因
stvec：trap handler 位址。

另外，trap flow 是這樣：

U-mode 執行 ecall
硬體切到 S-mode，寫 sepc、scause，跳到 stvec
start.S 存 32 個 general-purpose registers 到 trap frame
trap.c 的 do_trap 處理例外
start.S 還原暫存器
sret 回 U-mode。

所以 Exercise 4-1 實際上要你完成的是：

exec()：能載入 user program，準備好進 U-mode 執行
stvec 指到你的 trap entry
start.S 的 trap entry/return 流程要能正確存還原 context
do_trap() 要能辨識 ecall from U-mode，並印出 sepc/scause
最後輸出應該像投影片第 7 頁那樣，不斷看到 sepc、scause=8 一類訊息。
```



