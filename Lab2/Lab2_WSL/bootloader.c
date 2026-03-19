#include <stdint.h>
#include <stddef.h>

/*#define NULL ((void*)0)*/

/* =========================================================
 * Configuration
 * ========================================================= */
#define KERNEL_LOAD_ADDR   0x82000000UL   /* QEMU-safe example */
#define BOOT_MAGIC         0x544F4F42UL   /* "BOOT" little-endian */

#define FDT_MAGIC          0xd00dfeedU
#define FDT_BEGIN_NODE     0x00000001U
#define FDT_END_NODE       0x00000002U
#define FDT_PROP           0x00000003U
#define FDT_NOP            0x00000004U
#define FDT_END            0x00000009U

static uint64_t g_hartid;
static void *g_dtb;
static volatile uint8_t *g_uart_base = (volatile uint8_t *)0x10000000UL; /* fallback for QEMU */
static uint64_t g_kernel_entry = 0;
static uint64_t g_initrd_start = 0;
static uint64_t g_initrd_end = 0;

/* =========================================================
 * Basic helpers
 * ========================================================= */
static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *b && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static void memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

static int memcmp(const void *a, const void *b, size_t n) {
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

static uint64_t be64_to_cpu(uint64_t x) {
    return ((uint64_t)be32_to_cpu((uint32_t)(x >> 32))) |
           (((uint64_t)be32_to_cpu((uint32_t)(x & 0xffffffffU))) << 32);
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

static void put_hex_u64(uint64_t x);

/* =========================================================
 * UART (16550-style, QEMU virt)
 * ========================================================= */
#define UART_RBR 0   /* receive buffer */
#define UART_THR 0   /* transmit holding */
#define UART_LSR 5   /* line status */
#define UART_LSR_DR  0x01
#define UART_LSR_THRE 0x20

static inline uint8_t uart_read_reg(int off) {
    return g_uart_base[off];
}

static inline void uart_write_reg(int off, uint8_t val) {
    g_uart_base[off] = val;
}

static void uart_putc(char c) {
    if (c == '\n') {
        while ((uart_read_reg(UART_LSR) & UART_LSR_THRE) == 0) {}
        uart_write_reg(UART_THR, '\r');
    }
    while ((uart_read_reg(UART_LSR) & UART_LSR_THRE) == 0) {}
    uart_write_reg(UART_THR, (uint8_t)c);
    /*
    while ((uart_read_reg(UART_LSR) & UART_LSR_THRE) == 0) {}
    uart_write_reg(UART_THR, (uint8_t)c);
    if (c == '\n') {
        while ((uart_read_reg(UART_LSR) & UART_LSR_THRE) == 0) {}
        uart_write_reg(UART_THR, '\r');
    }
    */
}

static char uart_getc(void) {
    while ((uart_read_reg(UART_LSR) & UART_LSR_DR) == 0) {}
    return (char)uart_read_reg(UART_RBR);
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

/*
static void put_hex_u64(uint64_t x) {
    
    static const char *hex = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 15; i >= 0; i--) {
        uart_putc(hex[(x >> (i * 4)) & 0xf]);
    }
}
*/

static void put_hex_u64(uint64_t x) {
    static const char hex[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(hex[(x >> shift) & 0xFULL]);
    }
}


static void uart_getline(char *buf, int maxlen) {
    int i = 0;
    while (i < maxlen - 1) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            break;
        }
        if (c == 127 || c == '\b') {
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
            continue;
        }
        uart_putc(c);
        buf[i++] = c;
    }
    buf[i] = '\0';
}

/* =========================================================
 * FDT definitions
 * ========================================================= */
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

static int fdt_check_header(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return be32_to_cpu(h->magic) == FDT_MAGIC ? 0 : -1;
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

/* Return pointer to property value by full node path + property name */
static const void *fdt_getprop_by_path(const void *fdt, const char *target_path,
                                       const char *prop_name, int *lenp) {
    const uint32_t *p = fdt_struct(fdt);
    const char *strs = fdt_strings(fdt);
    const char *struct_end = (const char *)p + fdt_struct_size(fdt);

    char path[256];
    int depth = 0;
    path[0] = '\0';

    while ((const char *)p < struct_end) {
        uint32_t token = be32_to_cpu(*p++);
        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            size_t nlen = strlen(name);

            if (depth == 0) {
                path[0] = '/';
                path[1] = '\0';
            } else {
                if (strcmp(path, "/") != 0) {
                    size_t plen = strlen(path);
                    path[plen] = '/';
                    path[plen + 1] = '\0';
                }
                size_t plen = strlen(path);
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
                size_t plen = strlen(path);
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

            if (strcmp(path, target_path) == 0 && strcmp(name, prop_name) == 0) {
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

static int dtb_init(void) {
    if (fdt_check_header(g_dtb) < 0) return -1;

    int len = 0;
    const uint32_t *reg =
        (const uint32_t *)fdt_getprop_by_path(g_dtb, "/soc/serial", "reg", &len);
    /*
    if (reg && len >= 8) {
        // reg is usually addr,size ; cells vary by platform. For QEMU virt serial, this is enough for lab usage.
        uint64_t addr = ((uint64_t)be32_to_cpu(reg[0]) << 32) | be32_to_cpu(reg[1]);
        if (addr != 0) {
            g_uart_base = (volatile uint8_t *)(uintptr_t)addr;
        }
    }
    */

    if (reg && len >= 8) {
        uint64_t addr = ((uint64_t)be32_to_cpu(reg[0]) << 32) | be32_to_cpu(reg[1]);
        (void)addr;
        /* 暫時先不覆蓋 g_uart_base，先確認 dtb / initrd 功能穩定 */
    }
    

    const uint32_t *initrd_start =
        (const uint32_t *)fdt_getprop_by_path(g_dtb, "/chosen", "linux,initrd-start", &len);
    if (initrd_start && len >= 8) {
        g_initrd_start = ((uint64_t)be32_to_cpu(initrd_start[0]) << 32) |
                         be32_to_cpu(initrd_start[1]);
    } else if (initrd_start && len >= 4) {
        g_initrd_start = be32_to_cpu(initrd_start[0]);
    }

    const uint32_t *initrd_end =
        (const uint32_t *)fdt_getprop_by_path(g_dtb, "/chosen", "linux,initrd-end", &len);
    if (initrd_end && len >= 8) {
        g_initrd_end = ((uint64_t)be32_to_cpu(initrd_end[0]) << 32) |
                       be32_to_cpu(initrd_end[1]);
    } else if (initrd_end && len >= 4) {
        g_initrd_end = be32_to_cpu(initrd_end[0]);
    }

    return 0;
}

/* =========================================================
 * CPIO newc parser
 * ========================================================= */
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

static int cpio_valid_magic(const struct cpio_newc_header *h) {
    return memcmp(h->c_magic, "070701", 6) == 0;
}

static void cpio_ls(void) {
    if (g_initrd_start == 0 || g_initrd_end <= g_initrd_start) {
        uart_puts("initrd not found\n");
        return;
    }

    const uint8_t *p = (const uint8_t *)(uintptr_t)g_initrd_start;
    const uint8_t *end = (const uint8_t *)(uintptr_t)g_initrd_end;

    while (p + sizeof(struct cpio_newc_header) <= end) {
        const struct cpio_newc_header *h = (const struct cpio_newc_header *)p;
        if (!cpio_valid_magic(h)) {
            uart_puts("bad cpio magic\n");
            return;
        }

        uint32_t namesize = (uint32_t)hex_to_u64(h->c_namesize, 8);
        uint32_t filesize = (uint32_t)hex_to_u64(h->c_filesize, 8);

        const char *name = (const char *)(p + sizeof(struct cpio_newc_header));
        uint32_t name_area = align4((uint32_t)sizeof(struct cpio_newc_header) + namesize);
        const uint8_t *data = p + name_area;
        uint32_t data_area = align4(filesize);

        if (strcmp(name, "TRAILER!!!") == 0) {
            return;
        }

        uart_puts(name);
        uart_putc('\n');

        p = data + data_area;
    }
}

static int cpio_cat(const char *target) {
    if (g_initrd_start == 0 || g_initrd_end <= g_initrd_start) {
        uart_puts("initrd not found\n");
        return -1;
    }

    const uint8_t *p = (const uint8_t *)(uintptr_t)g_initrd_start;
    const uint8_t *end = (const uint8_t *)(uintptr_t)g_initrd_end;

    while (p + sizeof(struct cpio_newc_header) <= end) {
        const struct cpio_newc_header *h = (const struct cpio_newc_header *)p;
        if (!cpio_valid_magic(h)) {
            uart_puts("bad cpio magic\n");
            return -1;
        }

        uint32_t namesize = (uint32_t)hex_to_u64(h->c_namesize, 8);
        uint32_t filesize = (uint32_t)hex_to_u64(h->c_filesize, 8);

        const char *name = (const char *)(p + sizeof(struct cpio_newc_header));
        uint32_t name_area = align4((uint32_t)sizeof(struct cpio_newc_header) + namesize);
        const uint8_t *data = p + name_area;
        uint32_t data_area = align4(filesize);

        if (strcmp(name, "TRAILER!!!") == 0) {
            break;
        }

        if (strcmp(name, target) == 0 || (name[0] == '.' && name[1] == '/' && strcmp(name + 2, target) == 0)) {
            for (uint32_t i = 0; i < filesize; i++) uart_putc((char)data[i]);
            uart_putc('\n');
            return 0;
        }

        p = data + data_area;
    }

    uart_puts("file not found\n");
    return -1;
}

/* =========================================================
 * UART load protocol
 * header = little-endian u32 magic + u32 size
 * ========================================================= */
static uint32_t uart_recv_u32_le(void) {
    uint32_t v = 0;
    v |= (uint32_t)(uint8_t)uart_getc();
    v |= (uint32_t)(uint8_t)uart_getc() << 8;
    v |= (uint32_t)(uint8_t)uart_getc() << 16;
    v |= (uint32_t)(uint8_t)uart_getc() << 24;
    return v;
}

static int cmd_load(void) {
    uart_puts("waiting BOOT header...\n");

    uint32_t magic = uart_recv_u32_le();
    uint32_t size  = uart_recv_u32_le();

    if (magic != BOOT_MAGIC) {
        uart_puts("bad magic\n");
        return -1;
    }

    uart_puts("loading kernel size=");
    put_hex_u64(size);
    uart_putc('\n');

    uint8_t *dst = (uint8_t *)(uintptr_t)KERNEL_LOAD_ADDR;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = (uint8_t)uart_getc();
    }

    g_kernel_entry = KERNEL_LOAD_ADDR;

    uart_puts("kernel loaded at ");
    put_hex_u64(g_kernel_entry);
    uart_putc('\n');
    return 0;
}

/* =========================================================
 * Jump to kernel
 * RISC-V Linux expects:
 *   a0 = hartid
 *   a1 = dtb address
 *   satp = 0 before kernel
 * ========================================================= */
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
    if (g_kernel_entry == 0) {
        uart_puts("no kernel loaded\n");
        return;
    }
    uart_puts("jumping to kernel...\n");
    jump_to_kernel(g_kernel_entry, g_hartid, g_dtb);
}

/* =========================================================
 * Simple shell
 * ========================================================= */
static void print_help(void) {
    uart_puts("commands:\n");
    uart_puts("  help      - show help\n");
    uart_puts("  dtb       - show dtb/initrd info\n");
    uart_puts("  load      - load kernel over uart\n");
    uart_puts("  boot      - jump to loaded kernel\n");
    uart_puts("  ls        - list initramfs files\n");
    uart_puts("  cat FILE  - show file content from initramfs\n");
}

static void cmd_dtb(void) {
    uart_puts("hartid=");
    put_hex_u64(g_hartid);
    uart_putc('\n');

    uart_puts("dtb=");
    put_hex_u64((uint64_t)(uintptr_t)g_dtb);
    uart_putc('\n');

    uart_puts("uart_base=");
    put_hex_u64((uint64_t)(uintptr_t)g_uart_base);
    uart_putc('\n');

    uart_puts("initrd_start=");
    put_hex_u64(g_initrd_start);
    uart_putc('\n');

    uart_puts("initrd_end=");
    put_hex_u64(g_initrd_end);
    uart_putc('\n');
}

static void shell_loop(void) {
    char line[128];

    while (1) {
        uart_puts("boot> ");
        uart_getline(line, sizeof(line));

        if (strcmp(line, "help") == 0) {
            print_help();
        } else if (strcmp(line, "dtb") == 0) {
            cmd_dtb();
        } else if (strcmp(line, "load") == 0) {
            cmd_load();
        } else if (strcmp(line, "boot") == 0) {
            cmd_boot();
        } else if (strcmp(line, "ls") == 0) {
            cpio_ls();
        } else if (strncmp(line, "cat ", 4) == 0) {
            cpio_cat(line + 4);
        } else if (line[0] == '\0') {
            continue;
        } else {
            uart_puts("unknown command\n");
        }
    }
}

/* =========================================================
 * Entry
 * ========================================================= */
void boot_main(uint64_t hartid, void *dtb) {
    g_hartid = hartid;
    g_dtb = dtb;

    /* parse dtb first; if fail, fallback UART stays as default */
    if (dtb_init() < 0) {
        uart_puts("dtb parse failed, using fallback uart\n");
    }

    uart_puts("\n== Lab2 Bootloader ==\n");
    print_help();
    shell_loop();
}