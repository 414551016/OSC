#include "types.h"
#include "dtb.h"
#include "mm.h"
#include "printk.h"

/* ------------------------------------------------------------
 * kstrcmp:
 * 簡易字串比較函式
 * 回傳 0 表示兩字串完全相同
 * 主要用在 shell 指令判斷，例如 help / ex1 / all
 * ------------------------------------------------------------ */
static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ------------------------------------------------------------
 * console_getline:
 * 從主控台逐字讀入一整行
 * 功能：
 *   1. 支援 Enter 結束輸入
 *   2. 支援 Backspace 刪除字元
 *   3. 只接受可列印 ASCII 字元
 *   4. 最後補上 '\0'，讓結果成為 C 字串
 * 這是 shell 可以互動輸入的核心函式
 * ------------------------------------------------------------ */
static void console_getline(char *buf, int maxlen) {
    int i = 0;

    while (i < maxlen - 1) {
        int ch = console_getc();
        char c = (char)ch;

        /* 按下 Enter：結束本行輸入 */
        if (c == '\r' || c == '\n') {
            console_putc('\n');
            break;
        }

        /* 處理 Backspace / Delete */
        if (c == 127 || c == '\b') {
            if (i > 0) {
                i--;
                console_puts("\b \b");
            }
            continue;
        }

        /* 只接受可列印字元，並回顯到畫面 */
        if (c >= 32 && c <= 126) {
            console_putc(c);
            buf[i++] = c;
        }
    }

    /* 補上字串結尾 */
    buf[i] = '\0';
}

/* ------------------------------------------------------------
 * show_info:
 * 顯示目前 boot 資訊
 * 可用來展示 dtb、記憶體範圍、initrd 等資訊
 * ------------------------------------------------------------ */
static void show_info(struct boot_info *bi) {
    printk("\n[INFO]\n");
    printk("hartid       = 0\n");
    printk("dtb_ptr      = %p\n", bi->dtb);
    printk("mem.base     = 0x%lx\n", bi->mem.base);
    printk("mem.size     = 0x%lx\n", bi->mem.size);
    printk("dtb_size     = 0x%lx\n", bi->dtb_size);
    printk("initrd_start = 0x%lx\n", bi->initrd_start);
    printk("initrd_end   = 0x%lx\n", bi->initrd_end);
}

/* ------------------------------------------------------------
 * 以下 print_ex1 ~ print_adv3:
 * 只負責顯示各 exercise 的作業規定
 * 不做真正配置或釋放
 * 用途是讓使用者/TA 看作業需求說明
 * ------------------------------------------------------------ */
static void print_ex1(void) {
    printk("\n=============================================================\n");
    printk("Basic Exercise 1 - Buddy System\n");
    printk("=============================================================\n");
    printk("Goal:\n");
    printk("  Implement a buddy system page allocator.\n\n");

    printk("Requirements:\n");
    printk("  1. Initialize memory region from boot information.\n");
    printk("  2. Maintain free_area lists for each order.\n");
    printk("  3. Implement alloc_pages(order).\n");
    printk("  4. Split higher-order blocks when needed.\n");
    printk("  5. Implement free_pages(page).\n");
    printk("  6. Merge buddy blocks when possible.\n");
    printk("  7. Track metadata such as order and refcount.\n\n");

    printk("Demo expectations:\n");
    printk("  - Show free_area before allocation.\n");
    printk("  - Allocate blocks of different orders.\n");
    printk("  - Show split behavior.\n");
    printk("  - Free blocks and show merge behavior.\n");
    printk("  - Show free_area after free.\n");
}

static void print_ex2(void) {
    printk("\n=============================================================\n");
    printk("Basic Exercise 2 - Dynamic Memory Allocator\n");
    printk("=============================================================\n");
    printk("Goal:\n");
    printk("  Implement dynamic memory allocation on top of page allocator.\n\n");

    printk("Requirements:\n");
    printk("  1. Implement kmalloc(size).\n");
    printk("  2. Implement kfree(ptr).\n");
    printk("  3. Support small-size allocation using chunk/block pools.\n");
    printk("  4. Support large allocation using page allocator.\n");
    printk("  5. Organize memory by different size classes.\n");
    printk("  6. Recycle freed memory correctly.\n");
    printk("  7. Make allocator reusable for repeated allocations.\n\n");

    printk("Demo expectations:\n");
    printk("  - Allocate different chunk sizes.\n");
    printk("  - Free all allocated chunks.\n");
    printk("  - Show allocator status before/after operations.\n");
    printk("  - Demonstrate large allocation using pages.\n");
}

static void print_adv1(void) {
    printk("\n=============================================================\n");
    printk("Advanced Exercise 1 - Efficient Page Allocation\n");
    printk("=============================================================\n");
    printk("Goal:\n");
    printk("  Improve page allocation efficiency and reduce overhead.\n\n");

    printk("Requirements:\n");
    printk("  1. Optimize buddy allocation behavior.\n");
    printk("  2. Reduce unnecessary scanning overhead.\n");
    printk("  3. Improve free list management efficiency.\n");
    printk("  4. Keep allocation and free operations correct.\n");
    printk("  5. Preserve split/merge correctness while improving speed.\n\n");

    printk("Demo expectations:\n");
    printk("  - Explain what was optimized.\n");
    printk("  - Show that allocation/free still work correctly.\n");
    printk("  - Compare behavior before and after optimization if needed.\n");
}

static void print_adv2(void) {
    printk("\n=============================================================\n");
    printk("Advanced Exercise 2 - Reserved Memory\n");
    printk("=============================================================\n");
    printk("Goal:\n");
    printk("  Correctly handle reserved memory regions.\n\n");

    printk("Requirements:\n");
    printk("  1. Parse reserved memory information.\n");
    printk("  2. Mark reserved regions as unavailable.\n");
    printk("  3. Prevent allocator from using reserved pages.\n");
    printk("  4. Handle kernel image / dtb / initrd / device reserved areas.\n");
    printk("  5. Ensure reserved pages never enter free_area lists.\n\n");

    printk("Demo expectations:\n");
    printk("  - Print reserved memory regions.\n");
    printk("  - Show allocator excludes these regions.\n");
    printk("  - Explain which reserved ranges are protected.\n");
}

static void print_adv3(void) {
    printk("\n=============================================================\n");
    printk("Advanced Exercise 3 - Startup Allocation\n");
    printk("=============================================================\n");
    printk("Goal:\n");
    printk("  Implement startup-time allocation before normal allocator is ready.\n\n");

    printk("Requirements:\n");
    printk("  1. Implement startup_alloc(size, align).\n");
    printk("  2. Allocate early boot memory before buddy system is ready.\n");
    printk("  3. Respect alignment constraints.\n");
    printk("  4. Avoid overlap with reserved memory regions.\n");
    printk("  5. Use startup allocation for metadata such as mem_map.\n\n");

    printk("Demo expectations:\n");
    printk("  - Show startup allocation result.\n");
    printk("  - Explain alignment handling.\n");
    printk("  - Explain why startup_alloc is needed before mm_init completes.\n");
}

/* ------------------------------------------------------------
 * test_buddy:
 * Basic Exercise 1 demo
 * 展示 buddy allocator 的配置 / 釋放 / merge 結果
 * ------------------------------------------------------------ */
static void test_buddy(void) {
    printk("\n=== DEMO: Buddy Allocator ===\n");
    mm_dump();

    struct page *p1 = alloc_pages(0);
    struct page *p2 = alloc_pages(1);
    struct page *p3 = alloc_pages(2);

    printk("allocated p1=%p p2=%p p3=%p\n", p1, p2, p3);
    mm_dump();

    free_pages(p1);
    free_pages(p2);
    free_pages(p3);

    printk("after free_pages:\n");
    mm_dump();
}

/* ------------------------------------------------------------
 * test_kmalloc:
 * Basic Exercise 2 demo
 * 展示不同大小動態配置，以及 kfree 後是否正常回收
 * ------------------------------------------------------------ */
static void test_kmalloc(void) {
    printk("\n=== DEMO: Dynamic Memory Allocator ===\n");

    void *a = kmalloc(16);
    void *b = kmalloc(32);
    void *c = kmalloc(64);
    void *d = kmalloc(128);
    void *e = kmalloc(4000);
    void *f = kmalloc(8000);

    printk("kmalloc result a=%p b=%p c=%p d=%p e=%p f=%p\n",
           a, b, c, d, e, f);

    printk("allocator state after allocations:\n");
    mm_dump();

    kfree(a);
    kfree(b);
    kfree(c);
    kfree(d);
    kfree(e);
    kfree(f);

    printk("after kfree:\n");
    mm_dump();
}

/* ------------------------------------------------------------
 * stress_chunk_pool:
 * 額外壓力測試
 * 一次配置並釋放大量固定大小 chunk，觀察 pool 穩定性
 * ------------------------------------------------------------ */
static void stress_chunk_pool(void) {
    printk("\n=== DEMO: Chunk Pool Stress ===\n");

    void *arr[100];

    for (int i = 0; i < 100; i++) {
        arr[i] = kmalloc(128);
    }

    printk("allocated 100 chunks of 128 bytes\n");
    mm_dump();

    for (int i = 0; i < 100; i++) {
        kfree(arr[i]);
    }

    printk("freed 100 chunks of 128 bytes\n");
    mm_dump();
}

/* ------------------------------------------------------------
 * demo_adv1:
 * Advanced Exercise 1 demo
 * 用現有 buddy allocator 行為說明 efficient page allocation 概念
 * ------------------------------------------------------------ */
static void demo_adv1(void) {
    printk("\n=== DEMO: Advanced Exercise 1 - Efficient Page Allocation ===\n");
    printk("This demo shows current buddy allocator behavior.\n");
    printk("Efficient page allocation relies on split/merge with free_area lists.\n");
    printk("Below is a sample allocation/free sequence.\n");

    mm_dump();

    struct page *p1 = alloc_pages(0);
    struct page *p2 = alloc_pages(0);
    struct page *p3 = alloc_pages(1);
    struct page *p4 = alloc_pages(2);

    printk("allocated p1=%p p2=%p p3=%p p4=%p\n", p1, p2, p3, p4);
    mm_dump();

    free_pages(p2);
    free_pages(p1);
    free_pages(p3);
    free_pages(p4);

    printk("after freeing pages:\n");
    mm_dump();

    printk("Explanation:\n");
    printk("  - free_area lists allow fast lookup by order.\n");
    printk("  - higher-order blocks are split only when needed.\n");
    printk("  - freed buddies are merged back when possible.\n");
}

/* ------------------------------------------------------------
 * demo_adv2:
 * Advanced Exercise 2 demo
 * 展示哪些區域被視為 reserved memory，不可交給 allocator 使用
 * ------------------------------------------------------------ */
static void demo_adv2(struct boot_info *bi) {
    printk("\n=== DEMO: Advanced Exercise 2 - Reserved Memory ===\n");
    printk("Reserved memory handling demonstration.\n");

    printk("Protected regions include:\n");
    printk("  - DTB          : [%p, %p)\n",
           bi->dtb,
           (void *)((uintptr_t)bi->dtb + bi->dtb_size));

    printk("  - Kernel image : protected during mm_init\n");

    if (bi->initrd_end > bi->initrd_start) {
        printk("  - Initrd       : [0x%lx, 0x%lx)\n",
               bi->initrd_start, bi->initrd_end);
    } else {
        printk("  - Initrd       : not present\n");
    }

    printk("\nExplanation:\n");
    printk("  - Reserved regions are recorded before buddy initialization.\n");
    printk("  - Pages overlapping reserved regions are not inserted into free_area.\n");
    printk("  - Therefore allocator will not return reserved pages.\n");

    printk("\nCurrent free_area summary:\n");
    mm_dump();
}

/* ------------------------------------------------------------
 * demo_adv3:
 * Advanced Exercise 3 demo
 * 說明 startup_alloc 在正常 allocator 就緒前的用途
 * ------------------------------------------------------------ */
static void demo_adv3(void) {
    printk("\n=== DEMO: Advanced Exercise 3 - Startup Allocation ===\n");
    printk("startup_alloc() is used before the normal allocator is fully ready.\n");
    printk("In this lab, mem_map is allocated by startup_alloc during mm_init.\n");

    printk("\nDemonstration notes:\n");
    printk("  - startup_alloc(size, align) finds an aligned free range.\n");
    printk("  - it skips reserved regions.\n");
    printk("  - it is suitable for early metadata allocation.\n");
    printk("  - after mm_init, normal allocation should use buddy/kmalloc.\n");

    printk("\nObserved startup allocation result from boot log:\n");
    printk("  - mem_map was allocated during mm_init startup phase.\n");
    printk("  - see [MM] ... mem_map=... line printed at boot.\n");
}

/* ------------------------------------------------------------
 * run_all:
 * 依序執行完整展示流程
 * 適合口試 / demo / 驗收時一次跑完整套
 * ------------------------------------------------------------ */
static void run_all(struct boot_info *bi) {
    printk("\n=============================================================\n");
    printk("Running complete Lab3 demo sequence\n");
    printk("=============================================================\n");

    print_ex1();
    test_buddy();

    print_ex2();
    test_kmalloc();

    print_adv1();
    demo_adv1();

    print_adv2();
    demo_adv2(bi);

    print_adv3();
    demo_adv3();

    printk("\n=== EXTRA: Chunk Pool Stress ===\n");
    stress_chunk_pool();

    printk("\n=== ALL DEMOS DONE ===\n");
}

/* ------------------------------------------------------------
 * print_help:
 * 顯示所有支援的 shell 指令
 * ------------------------------------------------------------ */
static void print_help(void) {
    printk("\n=============================================================\n");
    printk("      NYCU OSC 2026 Lab3 Memory Allocator  ID = 414551016    \n");
    printk("=============================================================\n");

    printk("\ncommands:\n");
    printk("  help / ? - show help\n");
    printk("  info     - show boot info\n");

    printk("  ex1      - show Basic Exercise 1 requirements\n");
    printk("  buddy    - run Basic Exercise 1 demo\n");

    printk("  ex2      - show Basic Exercise 2 requirements\n");
    printk("  dynamic  - run Basic Exercise 2 demo\n");
    printk("  kmalloc  - run Basic Exercise 2 demo\n");

    printk("  adv1     - show Advanced Exercise 1 requirements\n");
    printk("  adv1demo - run Advanced Exercise 1 demo\n");

    printk("  adv2     - show Advanced Exercise 2 requirements\n");
    printk("  adv2demo - run Advanced Exercise 2 demo\n");

    printk("  adv3     - show Advanced Exercise 3 requirements\n");
    printk("  adv3demo - run Advanced Exercise 3 demo\n");

    printk("  dump     - show free_area status\n");
    printk("  stress   - run chunk pool stress demo\n");
    printk("  ta       - run NYCU TA demo\n");
    printk("  all      - run complete demo sequence\n");
}

/* ------------------------------------------------------------
 * shell_loop:
 * 互動式 shell 主迴圈
 * 流程：
 *   1. 顯示提示字元 lab3>
 *   2. 讀入一行命令
 *   3. 比對字串
 *   4. 呼叫對應功能
 * ------------------------------------------------------------ */


















/* ------------------------------------------------------------
 * main:
 * Kernel 進入點
 * 功能：
 *   1. 印出開機 banner
 *   2. 解析 DTB / boot info
 *   3. 初始化 memory allocator
 *   4. 啟動 interactive shell
 * ------------------------------------------------------------ */
void main(uint64_t hartid, void *dtb_ptr) {
    struct boot_info bi;

    printk("\n");
    printk("=============================================================\n");
    printk("      NYCU OSC 2026 Lab3 Memory Allocator  ID=414551016      \n");
    printk("=============================================================\n");
    printk("[EARLY] entered main()\n");
    printk("hartid  = %lu\n", (unsigned long)hartid);
    printk("dtb_ptr = %p\n", dtb_ptr);

    /* 解析 device tree，取得 boot info */
    dtb_init(dtb_ptr, &bi);
    printk("[EARLY] dtb_init ok\n");

    /* 顯示從 DTB 解析出的資訊 */
    printk("[DTB] mem.base      = 0x%lx\n", bi.mem.base);
    printk("[DTB] mem.size      = 0x%lx\n", bi.mem.size);
    printk("[DTB] dtb_size      = 0x%lx\n", bi.dtb_size);
    printk("[DTB] initrd_start  = 0x%lx\n", bi.initrd_start);
    printk("[DTB] initrd_end    = 0x%lx\n", bi.initrd_end);

    /* 初始化 buddy allocator / dynamic allocator 相關結構 */
    mm_init(&bi);


    

























    printk("[EARLY] mm_init ok\n");

    /* 啟動 shell */
    printk("\nLab3 interactive shell ready.\n");
    printk("Type 'help' to show commands.\n");

    





    shell_loop(&bi);
}