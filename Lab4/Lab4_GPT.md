> Q1：
> https://github.com/414551016/Operating-System-Capstone/blob/main/Lab4/Lab4.md
> https://github.com/riscv/riscv-isa-manual/releases 
> 1.請詳閱讀內容及參考檔案，請說明Lab4作業須要完成任務目標。 
> 2.我是初學者角色學習，請對以上「實作與寫作業的完整路線圖」步驟說明我該如何。

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

```
Exercise 4-2 的任務

投影片第 8～13 頁把這部分聚焦在 PLIC + UART interrupt：

了解 PLIC 是如何彙整、仲裁、轉送外部中斷到 hart
了解 PLIC_CLAIM / PLIC_COMPLETE 的 handling 模式
了解如何打開 UART interrupt。

投影片直接列的 TODO 是：

了解 plic_init() 如何設定 PLIC
了解 do_trap() 裡如何處理 PLIC interrupt
了解如何 enable UART interrupt。

投影片也給了關鍵知識：

PLIC 要設定 interrupt priority
要設定 interrupt enable
要設定 priority threshold。

而 UART interrupt 的啟用需要注意：

IER bit 0 = 1：接收緩衝收到資料時觸發 interrupt
MCR bit 3 (OUT2) = 1：讓 UART interrupt signal 能送到 PLIC。

最後預期結果是：你鍵盤打什麼，畫面就回顯什麼。
```
B. 再看 GitHub Lab4.md：完整作業版還包含 timer

GitHub 的 Lab4.md 比投影片更完整，列出這次 lab 的 goals 包含：

了解 RISC-V 的 exception mechanism
了解 OrangePi RV2 上 interrupt delegation 的概念
透過 SBI Timer Extension 設定並處理 core timer interrupts
透過 PLIC 處理 UART interrupts
學會 multiplex timers 與安排 asynchronous tasks。

也就是說，完整 Lab4 作業目標其實有三個 basic exercise + 進階題：

Basic Exercise 1 — Exception（30%）

內容是：

從 S-mode 切到 U-mode
設定 sepc
設定 sstatus
用 sret 跳到 U-mode
trap 回來時要：
stvec 指向 trap handler
保存 user context (x1-x31, sepc, sstatus)
印出 scause, sepc, stval
還原 context 後 sret 返回。
Basic Exercise 2 — Core Timer Interrupt（10%）

內容是：

rdtime 讀目前 timer
算出 2 秒後的 target time
呼叫 sbi_set_timer(target_time)
開 sie.STIE
開 sstatus.SIE
timer 來時由 scause 判斷
印出開機後經過幾秒
再設定下一次 2 秒後 timeout。
Basic Exercise 3 — UART0 Interrupt（30%）

內容是：

把原本 busy-wait 的 uart_getc / uart_puts 改成 interrupt-driven
用 ring buffer
開 UART interrupt register
在 PLIC enable 該 UART IRQ
開 sie.SEIE
開全域中斷
handler 中要：
RX：把收到的 byte 放進 buffer
TX：從 buffer 取資料送出
從 PLIC claim 取 IRQ，處理完再 complete 回去。
Advanced Exercise 1 — Timer Multiplexing（20%）

進階題是：

用 one-shot timer 做 software timer API
例如 add_timer(callback, arg, sec)
讓多個 timeout 事件可共用有限的硬體 timer。
```

```

