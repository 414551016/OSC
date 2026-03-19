#include <stdint.h>
#include <stddef.h>

#ifndef LOAD_ADDR
#define LOAD_ADDR 0x20000000UL
#endif

#define BOOT_MAGIC 0x544F4F42UL  /* "BOOT" */
#define FDT_MAGIC  0xd00dfeedU

#define FDT_BEGIN_NODE 0x00000001U
#define FDT_END_NODE   0x00000002U
#define FDT_PROP       0x00000003U
#define FDT_NOP        0x00000004U
#define FDT_END        0x00000009U

/* SBI legacy console extension */
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 1
#define SBI_EXT_0_1_CONSOLE_GETCHAR 2

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
};

struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

static uint64_t g_hartid;
static void *g_dtb;
static uint64_t g_initrd_start;
static uint64_t g_initrd_end;
static uint64_t g_kernel_entry;
static uint64_t g_uart_base;   /* parsed from DTB, display only */

/* ---------- SBI ---------- */

static inline long sbi_ecall(long ext, long fid,
                             long arg0, long arg1, long arg2,
                             long arg3, long arg4, long arg5)
{
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    register long a5 asm("a5") = arg5;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;

    asm volatile (
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );
    return a0;
}

static inline void sbi_console_putchar(int ch)
{
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

static inline int sbi_console_getchar(void)
{
    return (int)sbi_ecall(SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);
}

/* ---------- utils ---------- */

static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int kstrncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *b && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++;
        q++;
    }
    return 0;
}

static uint32_t be32_to_cpu(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

static uint32_t align4(uint32_t x) {
    return (x + 3U) & ~3U;
}

static uint64_t hex_to_u64(const char *s, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint64_t)(c - 'A' + 10);
    }
    return v;
}

/* ---------- console over SBI ---------- */

static void console_putc(char c) {
    if (c == '\n') sbi_console_putchar('\r');
    sbi_console_putchar((int)c);
}

static int console_getc_blocking(void) {
    int ch;
    do {
        ch = sbi_console_getchar();
    } while (ch == -1);
    return ch;
}

static void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

static void put_hex_u64(uint64_t x) {
    static const char hex[] = "0123456789abcdef";
    console_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        console_putc(hex[(x >> shift) & 0xFULL]);
    }
}

static void console_getline(char *buf, int maxlen) {
    int i = 0;
    while (i < maxlen - 1) {
        int ch = console_getc_blocking();
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

        console_putc(c);
        buf[i++] = c;
    }
    buf[i] = '\0';
}

/* ---------- fdt ---------- */

static int fdt_check_header(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return (be32_to_cpu(h->magic) == FDT_MAGIC) ? 0 : -1;
}

static const char *fdt_strings(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return (const char *)fdt + be32_to_cpu(h->off_dt_strings);
}

static const uint32_t *fdt_struct(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return (const uint32_t *)((const char *)fdt + be32_to_cpu(h->off_dt_struct));
}

static uint32_t fdt_struct_size(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return be32_to_cpu(h->size_dt_struct);
}

static const void *fdt_getprop_by_path(const void *fdt,
                                       const char *target_path,
                                       const char *prop_name,
                                       int *lenp) {
    const uint32_t *p = fdt_struct(fdt);
    const char *strs = fdt_strings(fdt);
    const char *end = (const char *)p + fdt_struct_size(fdt);

    char path[256];
    int depth = 0;
    path[0] = '\0';

    while ((const char *)p < end) {
        uint32_t token = be32_to_cpu(*p++);
        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            size_t nlen = kstrlen(name);

            if (depth == 0) {
                path[0] = '/';
                path[1] = '\0';
            } else {
                size_t plen = kstrlen(path);
                if (plen > 1) {
                    path[plen++] = '/';
                    path[plen] = '\0';
                }
                for (size_t i = 0; i < nlen; i++) path[plen + i] = name[i];
                path[plen + nlen] = '\0';
            }

            depth++;
            p = (const uint32_t *)((const char *)p + align4((uint32_t)nlen + 1U));
        } else if (token == FDT_END_NODE) {
            depth--;
            if (depth <= 0) {
                path[0] = '\0';
                depth = 0;
            } else {
                size_t plen = kstrlen(path);
                while (plen > 1 && path[plen - 1] != '/') plen--;
                if (plen > 1) path[plen - 1] = '\0';
                else {
                    path[0] = '/';
                    path[1] = '\0';
                }
            }
        } else if (token == FDT_PROP) {
            const struct fdt_prop *prop = (const struct fdt_prop *)p;
            uint32_t len = be32_to_cpu(prop->len);
            uint32_t nameoff = be32_to_cpu(prop->nameoff);
            const char *name = strs + nameoff;
            const void *value = (const char *)(prop + 1);

            if (kstrcmp(path, target_path) == 0 && kstrcmp(name, prop_name) == 0) {
                if (lenp) *lenp = (int)len;
                return value;
            }

            p = (const uint32_t *)((const char *)(prop + 1) + align4(len));
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            return NULL;
        }
    }

    return NULL;
}

static uint64_t fdt_reg_to_addr(const void *reg, int len) {
    const uint32_t *r = (const uint32_t *)reg;

    if (len >= 16) {
        return ((uint64_t)be32_to_cpu(r[0]) << 32) | be32_to_cpu(r[1]);
    }
    if (len >= 8) {
        return ((uint64_t)be32_to_cpu(r[0]) << 32) | be32_to_cpu(r[1]);
    }
    if (len >= 4) {
        return be32_to_cpu(r[0]);
    }
    return 0;
}

static int dtb_init(void) {
    int len = 0;
    const void *reg = NULL;

    if (fdt_check_header(g_dtb) < 0) return -1;

    /* For display/debug only */
    reg = fdt_getprop_by_path(g_dtb, "/soc/serial", "reg", &len);
    if (!reg)
        reg = fdt_getprop_by_path(g_dtb, "/soc/uart", "reg", &len);
    if (!reg)
        reg = fdt_getprop_by_path(g_dtb, "/soc/serial@d4017000", "reg", &len);
    if (!reg)
        reg = fdt_getprop_by_path(g_dtb, "/soc/uart@d4017000", "reg", &len);

    if (reg) {
        g_uart_base = fdt_reg_to_addr(reg, len);
    }

    reg = fdt_getprop_by_path(g_dtb, "/chosen", "linux,initrd-start", &len);
    if (reg) g_initrd_start = fdt_reg_to_addr(reg, len);

    reg = fdt_getprop_by_path(g_dtb, "/chosen", "linux,initrd-end", &len);
    if (reg) g_initrd_end = fdt_reg_to_addr(reg, len);

    return 0;
}

/* ---------- cpio ---------- */

static int cpio_valid_magic(const struct cpio_newc_header *h) {
    return kmemcmp(h->c_magic, "070701", 6) == 0;
}

static void cpio_ls(void) {
    if (!g_initrd_start || g_initrd_end <= g_initrd_start) {
        console_puts("initrd not found\n");
        return;
    }

    const uint8_t *p = (const uint8_t *)(uintptr_t)g_initrd_start;
    const uint8_t *end = (const uint8_t *)(uintptr_t)g_initrd_end;

    while (p + sizeof(struct cpio_newc_header) <= end) {
        const struct cpio_newc_header *h = (const struct cpio_newc_header *)p;
        if (!cpio_valid_magic(h)) {
            console_puts("bad cpio magic\n");
            return;
        }

        uint32_t namesize = (uint32_t)hex_to_u64(h->c_namesize, 8);
        uint32_t filesize = (uint32_t)hex_to_u64(h->c_filesize, 8);

        const char *name = (const char *)(p + sizeof(struct cpio_newc_header));
        uint32_t name_area = align4((uint32_t)sizeof(struct cpio_newc_header) + namesize);
        const uint8_t *data = p + name_area;
        uint32_t data_area = align4(filesize);

        if (kstrcmp(name, "TRAILER!!!") == 0)
            return;

        console_puts(name);
        console_putc('\n');

        p = data + data_area;
    }
}

static int cpio_cat(const char *target) {
    if (!g_initrd_start || g_initrd_end <= g_initrd_start) {
        console_puts("initrd not found\n");
        return -1;
    }

    const uint8_t *p = (const uint8_t *)(uintptr_t)g_initrd_start;
    const uint8_t *end = (const uint8_t *)(uintptr_t)g_initrd_end;

    while (p + sizeof(struct cpio_newc_header) <= end) {
        const struct cpio_newc_header *h = (const struct cpio_newc_header *)p;
        if (!cpio_valid_magic(h)) {
            console_puts("bad cpio magic\n");
            return -1;
        }

        uint32_t namesize = (uint32_t)hex_to_u64(h->c_namesize, 8);
        uint32_t filesize = (uint32_t)hex_to_u64(h->c_filesize, 8);

        const char *name = (const char *)(p + sizeof(struct cpio_newc_header));
        uint32_t name_area = align4((uint32_t)sizeof(struct cpio_newc_header) + namesize);
        const uint8_t *data = p + name_area;
        uint32_t data_area = align4(filesize);

        if (kstrcmp(name, "TRAILER!!!") == 0)
            break;

        if (kstrcmp(name, target) == 0 ||
            (name[0] == '.' && name[1] == '/' && kstrcmp(name + 2, target) == 0)) {
            for (uint32_t i = 0; i < filesize; i++) console_putc((char)data[i]);
            console_putc('\n');
            return 0;
        }

        p = data + data_area;
    }

    console_puts("file not found\n");
    return -1;
}

/* ---------- uart load over SBI console ---------- */

static uint32_t console_recv_u32_le(void) {
    uint32_t v = 0;
    v |= (uint32_t)(uint8_t)console_getc_blocking();
    v |= (uint32_t)(uint8_t)console_getc_blocking() << 8;
    v |= (uint32_t)(uint8_t)console_getc_blocking() << 16;
    v |= (uint32_t)(uint8_t)console_getc_blocking() << 24;
    return v;
}

static int cmd_load(void) {
    console_puts("waiting BOOT header...\n");

    uint32_t magic = console_recv_u32_le();
    uint32_t size  = console_recv_u32_le();

    if (magic != BOOT_MAGIC) {
        console_puts("bad magic\n");
        return -1;
    }

    console_puts("loading kernel size=");
    put_hex_u64(size);
    console_putc('\n');

    uint8_t *dst = (uint8_t *)(uintptr_t)LOAD_ADDR;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = (uint8_t)console_getc_blocking();
    }

    g_kernel_entry = LOAD_ADDR;

    console_puts("kernel loaded at ");
    put_hex_u64(g_kernel_entry);
    console_putc('\n');
    return 0;
}

/* ---------- jump ---------- */

static void jump_to_kernel(uint64_t entry, uint64_t hartid, void *dtb) {
    register uint64_t a0 asm("a0") = hartid;
    register void *a1 asm("a1") = dtb;

    asm volatile(
        "csrw satp, zero\n"
        "jr %0\n"
        :
        : "r"(entry), "r"(a0), "r"(a1)
        : "memory"
    );
}

static void cmd_boot(void) {
    if (!g_kernel_entry) {
        console_puts("no kernel loaded\n");
        return;
    }
    console_puts("jumping to kernel...\n");
    jump_to_kernel(g_kernel_entry, g_hartid, g_dtb);
}

/* ---------- shell ---------- */

static void print_help(void) {
    console_puts("commands:\n");
    console_puts("  help      - show help\n");
    console_puts("  dtb       - show dtb/initrd info\n");
    console_puts("  load      - load kernel over uart\n");
    console_puts("  boot      - jump to loaded kernel\n");
    console_puts("  ls        - list initramfs files\n");
    console_puts("  cat FILE  - show file content from initramfs\n");
}

static void cmd_dtb(void) {
    console_puts("hartid=");       put_hex_u64(g_hartid);                    console_putc('\n');
    console_puts("dtb=");          put_hex_u64((uint64_t)(uintptr_t)g_dtb); console_putc('\n');
    console_puts("uart_base=");    put_hex_u64(g_uart_base);                 console_putc('\n');
    console_puts("initrd_start="); put_hex_u64(g_initrd_start);              console_putc('\n');
    console_puts("initrd_end=");   put_hex_u64(g_initrd_end);                console_putc('\n');
    console_puts("load_addr=");    put_hex_u64((uint64_t)LOAD_ADDR);         console_putc('\n');
}

static void shell_loop(void) {
    char line[128];

    while (1) {
        console_puts("boot> ");
        console_getline(line, sizeof(line));

        if (kstrcmp(line, "help") == 0) {
            print_help();
        } else if (kstrcmp(line, "dtb") == 0) {
            cmd_dtb();
        } else if (kstrcmp(line, "load") == 0) {
            cmd_load();
        } else if (kstrcmp(line, "boot") == 0) {
            cmd_boot();
        } else if (kstrcmp(line, "ls") == 0) {
            cpio_ls();
        } else if (kstrncmp(line, "cat ", 4) == 0) {
            cpio_cat(line + 4);
        } else if (line[0] == '\0') {
            continue;
        } else {
            console_puts("unknown command\n");
        }
    }
}

void boot_main(uint64_t hartid, void *dtb) {
    g_hartid = hartid;
    g_dtb = dtb;
    g_uart_base = 0;

    if (dtb_init() < 0) {
        console_puts("dtb parse failed\n");
    }

    console_puts("\n== OSC Lab2 Bootloader ID：414551016 ==\n");
    print_help();
    shell_loop();
}