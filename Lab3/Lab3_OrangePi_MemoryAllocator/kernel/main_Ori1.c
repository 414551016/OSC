#include "types.h"
#include "dtb.h"
#include "mm.h"
#include "printk.h"

static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static void console_getline(char *buf, int maxlen) {
    int i = 0;

    while (i < maxlen - 1) {
        int ch = console_getc();
        char c = (char)ch;

        if (c == '\r' || c == '\n') {
            console_putc('\n');
            break;
        }

        if (c == 127 || c == '\b') {
            if (i > 0) {
                i--;
                console_puts("\b \b");
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            console_putc(c);
            buf[i++] = c;
        }
    }

    buf[i] = '\0';
}

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

static void test_kmalloc(void) {
    printk("\n=== DEMO: kmalloc / kfree ===\n");

    void *a = kmalloc(16);
    void *b = kmalloc(32);
    void *c = kmalloc(64);
    void *d = kmalloc(128);
    void *e = kmalloc(4000);
    void *f = kmalloc(8000);

    printk("kmalloc result a=%p b=%p c=%p d=%p e=%p f=%p\n",
           a, b, c, d, e, f);

    kfree(a);
    kfree(b);
    kfree(c);
    kfree(d);
    kfree(e);
    kfree(f);

    printk("after kfree:\n");
    mm_dump();
}

static void stress_chunk_pool(void) {
    printk("\n=== DEMO: Chunk Pool Stress ===\n");

    void *arr[100];

    for (int i = 0; i < 100; i++) {
        arr[i] = kmalloc(128);
    }

    printk("allocated 100 chunks of 128 bytes\n");

    for (int i = 0; i < 100; i++) {
        kfree(arr[i]);
    }

    printk("freed 100 chunks of 128 bytes\n");
    mm_dump();
}

static void run_all(void) {
    test_buddy();
    test_kmalloc();
    stress_chunk_pool();
    printk("\n=== DEMO DONE ===\n");
}

static void print_help(void) {
    printk("===============================================================\n");
    printk("      NYCU OSC 2026 Lab3 Memory Allocator  ID = 414551016      \n");
    printk("===============================================================\n");
    printk("\ncommands:\n");
    printk("  help / ? - show help\n");
    printk("  info     - show boot info\n");
    printk("  ex1      - show Basic Exercise 1 - Buddy System\n");
    printk("  buddy    - run buddy allocator demo\n");
    printk("  kmalloc  - run kmalloc/kfree demo\n");
    printk("  stress   - run chunk pool stress demo\n");
    printk("  all      - run all demos\n");
}

static void print_ex1(void) {
    printk("\n=== Basic Exercise 1: Buddy System ===\n");

    printk("Implement a buddy system memory allocator.\n\n");

    printk("[Requirements]\n");
    printk("1. Initialize memory region using boot information.\n");
    printk("2. Maintain free_area lists for each order.\n");
    printk("3. Support page allocation:\n");
    printk("   - alloc_pages(order)\n");
    printk("   - split higher order blocks when needed\n");
    printk("4. Support page free:\n");
    printk("   - free_pages(page)\n");
    printk("   - merge with buddy if possible\n");
    printk("5. Track page metadata:\n");
    printk("   - order\n");
    printk("   - refcount\n\n");

    printk("[Demo Expectations]\n");
    printk("- Show free_area before allocation\n");
    printk("- Allocate pages with different orders\n");
    printk("- Show splitting process\n");
    printk("- Free pages and show merging\n");
    printk("- Show free_area after free\n");

    printk("=======================================\n");
}

static void shell_loop(struct boot_info *bi) {
    char line[64];

    while (1) {
        printk("lab3> ");
        console_getline(line, sizeof(line));

        if (line[0] == '\0') {
            continue;
        } else if (kstrcmp(line, "help") == 0 || kstrcmp(line, "?") == 0) {
            print_help();
        } else if (kstrcmp(line, "info") == 0) {
            show_info(bi);
        } else if (kstrcmp(line, "ex1") == 0) {
            print_ex1();
        } else if (kstrcmp(line, "buddy") == 0) {
            test_buddy();
        } else if (kstrcmp(line, "dump") == 0) {
            mm_dump();
        } else if (kstrcmp(line, "kmalloc") == 0) {
            test_kmalloc();
        } else if (kstrcmp(line, "stress") == 0) {
            stress_chunk_pool();
        } else if (kstrcmp(line, "all") == 0) {
            run_all();
        } else {
            printk("unknown command: %s\n", line);
            printk("type 'help' for available commands\n");
        }
    }
}

void main(uint64_t hartid, void *dtb_ptr) {
    struct boot_info bi;

    printk("\n");
    printk("=============================================================\n");
    printk("      NYCU OSC 2026 Lab3 Memory Allocator  ID=414551016      \n");
    printk("=============================================================\n");
    printk("[EARLY] entered main()\n");
    printk("hartid  = %lu\n", (unsigned long)hartid);
    printk("dtb_ptr = %p\n", dtb_ptr);

    dtb_init(dtb_ptr, &bi);
    printk("[EARLY] dtb_init ok\n");

    printk("[DTB] mem.base      = 0x%lx\n", bi.mem.base);
    printk("[DTB] mem.size      = 0x%lx\n", bi.mem.size);
    printk("[DTB] dtb_size      = 0x%lx\n", bi.dtb_size);
    printk("[DTB] initrd_start  = 0x%lx\n", bi.initrd_start);
    printk("[DTB] initrd_end    = 0x%lx\n", bi.initrd_end);

    mm_init(&bi);
    printk("[EARLY] mm_init ok\n");

    printk("\nLab3 interactive shell ready.\n");
    printk("Type 'help' to show commands.\n");

    shell_loop(&bi);
}