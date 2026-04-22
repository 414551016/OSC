extern char uart_getc(void);
extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);
extern int hextoi(const char* s, int n);
extern int align(int n, int byte);
extern int memcmp(const void* s1, const void* s2, int n);
extern void* alloc_page();

extern unsigned long dtb_ptr;

#define STACK_SIZE  0x1000

#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  0x1U
#define FDT_END_NODE    0x2U
#define FDT_PROP        0x3U
#define FDT_NOP         0x4U
#define FDT_END         0x9U

struct fdt_header {
    unsigned int magic;
    unsigned int totalsize;
    unsigned int off_dt_struct;
    unsigned int off_dt_strings;
    unsigned int off_mem_rsvmap;
    unsigned int version;
    unsigned int last_comp_version;
    unsigned int boot_cpuid_phys;
    unsigned int size_dt_strings;
    unsigned int size_dt_struct;
};

struct cpio_t {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
};

struct pt_regs {
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;
    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    unsigned long s0;
    unsigned long s1;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;
    unsigned long sepc;
    unsigned long sstatus;
    unsigned long scause;
    unsigned long stval;
};

static unsigned int be32(const void* p) {
    const unsigned char* b = (const unsigned char*)p;
    return ((unsigned int)b[0] << 24) |
           ((unsigned int)b[1] << 16) |
           ((unsigned int)b[2] <<  8) |
           ((unsigned int)b[3] <<  0);
}

static unsigned long be64(const void* p) {
    const unsigned char* b = (const unsigned char*)p;
    return ((unsigned long)b[0] << 56) |
           ((unsigned long)b[1] << 48) |
           ((unsigned long)b[2] << 40) |
           ((unsigned long)b[3] << 32) |
           ((unsigned long)b[4] << 24) |
           ((unsigned long)b[5] << 16) |
           ((unsigned long)b[6] <<  8) |
           ((unsigned long)b[7] <<  0);
}

static int strcmp_simple(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b)
            return (unsigned char)*a - (unsigned char)*b;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static void memcpy_simple(void* dst, const void* src, int n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    while (n-- > 0)
        *d++ = *s++;
}

static int c_strlen(const char* s) {
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

static int streq_cpio_name(const char* entry_name, int namesize, const char* target) {
    int i = 0;

    while (i < namesize && target[i] != '\0') {
        if (entry_name[i] != target[i])
            return 0;
        i++;
    }

    if (target[i] != '\0')
        return 0;
    if (i >= namesize)
        return 0;

    return entry_name[i] == '\0';
}

static unsigned long get_initrd_start_from_dtb(void) {
    const unsigned char* dtb = (const unsigned char*)dtb_ptr;
    const struct fdt_header* hdr = (const struct fdt_header*)dtb;
    unsigned int off_struct;
    unsigned int off_strings;
    const unsigned char* p;
    const char* strings;
    int chosen_depth = 0;
    int depth = 0;

    if (be32(&hdr->magic) != FDT_MAGIC)
        return 0;

    off_struct = be32(&hdr->off_dt_struct);
    off_strings = be32(&hdr->off_dt_strings);

    p = dtb + off_struct;
    strings = (const char*)(dtb + off_strings);

    while (1) {
        unsigned int token = be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            const char* name = (const char*)p;
            depth++;
            if (depth == 2 && strcmp_simple(name, "chosen") == 0)
                chosen_depth = depth;

            p += align(c_strlen(name) + 1, 4);
        } else if (token == FDT_END_NODE) {
            if (depth == chosen_depth)
                chosen_depth = 0;
            depth--;
        } else if (token == FDT_PROP) {
            unsigned int len = be32(p); p += 4;
            unsigned int nameoff = be32(p); p += 4;
            const char* propname = strings + nameoff;

            if (chosen_depth && strcmp_simple(propname, "linux,initrd-start") == 0) {
                if (len == 8)
                    return be64(p);
                if (len == 4)
                    return be32(p);
            }

            p += align((int)len, 4);
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            break;
        }
    }

    return 0;
}

static void enter_user_mode(void* entry, void* user_sp) __attribute__((noreturn));
static void enter_user_mode(void* entry, void* user_sp) {
    asm volatile(
        "csrw sepc, %[entry]\n"
        "csrw sscratch, sp\n"
        "mv sp, %[usp]\n"
        "csrr t0, sstatus\n"
        "li t1, ~(1 << 8)\n"
        "and t0, t0, t1\n"
        "li t1, (1 << 5)\n"
        "or t0, t0, t1\n"
        "csrw sstatus, t0\n"
        "sret\n"
        :
        : [entry] "r"(entry), [usp] "r"(user_sp)
        : "t0", "t1", "memory"
    );

    while (1) { }
}

int exec(const char* filename) {
    unsigned long initrd_base = get_initrd_start_from_dtb();
    char* p;

    if (!initrd_base)
        return -1;

    p = (char*)initrd_base;

    while (memcmp(p + sizeof(struct cpio_t), "TRAILER!!!", 10)) {
        struct cpio_t* hdr = (struct cpio_t*)p;
        int namesize = hextoi(hdr->namesize, 8);
        int filesize = hextoi(hdr->filesize, 8);
        int headsize = align((int)sizeof(struct cpio_t) + namesize, 4);
        int datasize = align(filesize, 4);
        char* name = p + sizeof(struct cpio_t);
        char* data = p + headsize;

        if (streq_cpio_name(name, namesize, filename)) {
            char* user_prog = (char*)alloc_page();
            char* user_stack_base = (char*)alloc_page();
            char* user_sp = user_stack_base + STACK_SIZE;

            memcpy_simple(user_prog, data, filesize);
            enter_user_mode(user_prog, user_sp);
        }

        p += headsize + datasize;
    }

    return -1;
}

void do_trap(struct pt_regs* regs) {
    uart_puts("=== S-Mode trap ===\n");
    uart_puts("scause: ");
    uart_hex(regs->scause);
    uart_puts("\n");
    uart_puts("sepc: ");
    uart_hex(regs->sepc);
    uart_puts("\n");
    uart_puts("stval: ");
    uart_hex(regs->stval);
    uart_puts("\n");

    if (regs->scause == 8) {
        regs->sepc += 4;
        return;
    }

    while (1) { }
}

void start_kernel() {
    uart_puts("\nStarting kernel ...\n");
    if (exec("prog.bin"))
        uart_puts("Failed to exec user program!\n");

    while (1) {
        uart_putc(uart_getc());
    }
}