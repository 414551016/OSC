#include "common.h"
#include "uart.h"
#include "printk.h"
#include "startup_alloc.h"

/* linker script 匯出的 kernel 結尾符號 */
extern char __kernel_end[];

/* =========================================================
 * 最小可驗證版：
 * 1. 初始化 UART
 * 2. 建立 startup allocator
 * 3. 做幾次配置測試
 * 4. 印出結果
 * =========================================================
 */
void kernel_main(void)
{
    uart_init();

    printk("\n==============================\n");
    printk(" OSC Lab3 minimal kernel boot\n");
    printk("==============================\n");

    /* 假設 kernel 後面有一段可用記憶體作為 early heap
     * 這裡先示範保留 1MB 給 startup allocator
     */
    void *heap_start = (void *)ALIGN_UP((u64)__kernel_end, 4096);
    void *heap_end   = (void *)((u64)heap_start + 1024 * 1024);

    startup_alloc_init(heap_start, heap_end);

    printk("[startup] heap_start = %x\n", (u64)heap_start);
    printk("[startup] heap_end   = %x\n", (u64)heap_end);

    void *p1 = startup_alloc(64, 16);
    void *p2 = startup_alloc(100, 32);
    void *p3 = startup_alloc(4096, 4096);

    printk("[startup] p1 = %x\n", (u64)p1);
    printk("[startup] p2 = %x\n", (u64)p2);
    printk("[startup] p3 = %x\n", (u64)p3);

    printk("[startup] used      = %d bytes\n", startup_alloc_used());
    printk("[startup] remaining = %d bytes\n", startup_alloc_remaining());

    printk("[ok] minimal version works.\n");

    while (1) {
        asm volatile("wfi");
    }
}