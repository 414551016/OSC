/* bootloader 做的事情：
1,初始化
    取得 hartid、dtb
    解析 device tree（DTB）
2, 提供 console（用 SBI）
    putc / gets / puts
3,提供 shell 指令
    help：顯示指令
    load：透過 UART 載入 kernel
    boot：跳到 kernel
    ls / cat：讀 initramfs（cpio）
4, kernel 啟動流程
UART 傳 kernel
     ↓
存到 LOAD_ADDR
     ↓
設 g_kernel_entry
     ↓
jump_to_kernel()
*/

#include <stdint.h>   // 定義固定大小整數 (uint32_t, uint64_t 等)
#include <stddef.h>   // 定義 size_t 等型別

// 預設 kernel 載入地址，如果 Makefile 定義了就用 Makefile 的，沒有就用 0x20000000
#ifndef LOAD_ADDR
#define LOAD_ADDR 0x20000000UL 
#endif


#define BOOT_MAGIC 0x544F4F42UL  /* "BOOT" magic number，用來驗證傳輸資料 */
#define FDT_MAGIC  0xd00dfeedU   // Device Tree Blob (DTB) magic number

// FDT (Device Tree) token 定義
#define FDT_BEGIN_NODE 0x00000001U
#define FDT_END_NODE   0x00000002U
#define FDT_PROP       0x00000003U
#define FDT_NOP        0x00000004U
#define FDT_END        0x00000009U

/* SBI (Supervisor Binary Interface) console extension */
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 1  // 輸出字元
#define SBI_EXT_0_1_CONSOLE_GETCHAR 2  // 讀取字元

/* --------- FDT 結構 --------- */
struct fdt_header {
    uint32_t magic;              // magic number
    uint32_t totalsize;          // 整個 DTB 大小
    uint32_t off_dt_struct;      // structure 區偏移
    uint32_t off_dt_strings;     // strings 區偏移
    uint32_t off_mem_rsvmap;     // reserved memory
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_prop {
    uint32_t len;       // property 長度
    uint32_t nameoff;   // name 在 string table 的 offset
};

/* --------- CPIO (initramfs) header --------- */
struct cpio_newc_header {
    char c_magic[6];     // magic ("070701")
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];  // 檔案大小（ASCII hex）
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];  // 檔名長度
    char c_check[8];
};

/* --------- 全域變數 --------- */
static uint64_t g_hartid;         // CPU hart id
static void *g_dtb;               // device tree pointer
static uint64_t g_initrd_start;   // initramfs 起點
static uint64_t g_initrd_end;     // initramfs 結尾
static uint64_t g_kernel_entry;   // kernel entry address
static uint64_t g_uart_base;      // UART base address（從 DTB 解析）

/* ================= SBI 呼叫 ================= */

// 呼叫 SBI (透過 ecall 指令)
static inline long sbi_ecall(long ext, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5)
{
    // 將參數放入 RISC-V 暫存器
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    register long a5 asm("a5") = arg5;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;

    asm volatile (
        "ecall"   // 進入 SBI
        : "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );
    return a0;  // 回傳結果
}

// 輸出一個字元
static inline void sbi_console_putchar(int ch)
{
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

// 讀取一個字元
static inline int sbi_console_getchar(void)
{
    return (int)sbi_ecall(SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);
}

/* ================= 基本工具函式 ================= */

// 計算字串長度
static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

// 字串比較
static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

// 前 n 個字元比較
static int kstrncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *b && *a == *b) {
        a++; b++; n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

// 記憶體比較
static int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a;
    const uint8_t *q = b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

// big endian → CPU endian
static uint32_t be32_to_cpu(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

// 4-byte 對齊
static uint32_t align4(uint32_t x) {
    return (x + 3U) & ~3U;
}

// ASCII hex → 整數
static uint64_t hex_to_u64(const char *s, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (c - '0');
        else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
    }
    return v;
}

/* ================= Console ================= */

// 輸出字元（處理換行）
static void console_putc(char c) {
    if (c == '\n') sbi_console_putchar('\r'); // 換行補 CR
    sbi_console_putchar((int)c);
}

// 阻塞讀字元
static int console_getc_blocking(void) {
    int ch;
    do {
        ch = sbi_console_getchar();
    } while (ch == -1);  // 沒資料就一直等
    return ch;
}

// 輸出字串
static void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

// 輸出 64-bit hex
static void put_hex_u64(uint64_t x) {
    static const char hex[] = "0123456789abcdef";
    console_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        console_putc(hex[(x >> shift) & 0xF]);
    }
}

/* ================= Boot 指令 ================= */

// 從 UART 收 kernel
static int cmd_load(void) {
    console_puts("waiting BOOT header...\n");

    uint32_t magic = console_recv_u32_le(); // 讀 magic
    uint32_t size  = console_recv_u32_le(); // 讀大小

    if (magic != BOOT_MAGIC) {
        console_puts("bad magic\n"); // 驗證失敗
        return -1;
    }

    console_puts("loading kernel size=");
    put_hex_u64(size);
    console_putc('\n');

    uint8_t *dst = (uint8_t *)(uintptr_t)LOAD_ADDR;

    // 逐 byte 接收 kernel
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = console_getc_blocking();
    }

    g_kernel_entry = LOAD_ADDR; // 設定 entry

    console_puts("kernel loaded at ");
    put_hex_u64(g_kernel_entry);
    console_putc('\n');
    return 0;
}

/* 跳到 kernel */
static void jump_to_kernel(uint64_t entry, uint64_t hartid, void *dtb) {
    register uint64_t a0 asm("a0") = hartid; // 傳 hartid
    register void *a1 asm("a1") = dtb;       // 傳 dtb

    asm volatile(
        "csrw satp, zero\n" // 關閉 paging
        "jr %0\n"           // 跳到 kernel entry
        :
        : "r"(entry), "r"(a0), "r"(a1)
        : "memory"
    );
}

/* boot 指令 */
static void cmd_boot(void) {
    if (!g_kernel_entry) {
        console_puts("no kernel loaded\n");
        return;
    }
    console_puts("jumping to kernel...\n");
    jump_to_kernel(g_kernel_entry, g_hartid, g_dtb);
}

/* ================= shell ================= */

// 顯示指令
static void print_help(void) {
    console_puts("commands:\n");
    console_puts("  help\n");
    console_puts("  dtb\n");
    console_puts("  load\n");
    console_puts("  boot\n");
    console_puts("  ls\n");
    console_puts("  cat FILE\n");
}

/* shell 主迴圈 */
static void shell_loop(void) {
    char line[128];

    while (1) {
        console_puts("boot> ");
        console_getline(line, sizeof(line));

        if (kstrcmp(line, "help") == 0) {
            print_help();
        } else if (kstrcmp(line, "load") == 0) {
            cmd_load();
        } else if (kstrcmp(line, "boot") == 0) {
            cmd_boot();
        } else {
            console_puts("unknown command\n");
        }
    }
}

/* bootloader entry */
void boot_main(uint64_t hartid, void *dtb) {
    g_hartid = hartid; // 儲存 hart id
    g_dtb = dtb;       // 儲存 DTB

    console_puts("\n== Bootloader ==\n");
    print_help();
    shell_loop();      // 進入 shell
}
