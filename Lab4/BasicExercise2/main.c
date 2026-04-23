extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);

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

#define TIMER_FREQ     10000000UL
#define INTERVAL_SEC   2UL
#define TIMER_INTERVAL (TIMER_FREQ * INTERVAL_SEC)

static unsigned long long next_deadline = 0;
static unsigned long boot_time_sec = 0;

static inline unsigned long long rdtime64(void) {
    unsigned long t;
    asm volatile("rdtime %0" : "=r"(t));
    return (unsigned long long)t;
}

static inline void sbi_set_timer(unsigned long long stime_value) {
    register unsigned long a0 asm("a0") = (unsigned long)stime_value;
    register unsigned long a1 asm("a1") = (unsigned long)(stime_value >> 32);
    register unsigned long a6 asm("a6") = 0;
    register unsigned long a7 asm("a7") = 0x54494D45UL;

    asm volatile(
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a6), "r"(a7)
        : "memory");
}

static void uart_put_dec(unsigned long x) {
    char buf[32];
    int i = 0;

    if (x == 0) {
        uart_putc('0');
        return;
    }

    while (x > 0) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }

    while (i > 0)
        uart_putc(buf[--i]);
}

void enable_timer_interrupt(void) {
    asm volatile(
        "li t0, (1 << 5)\n"
        "csrs sie, t0\n");
}

void irq_enable(void) {
    asm volatile("csrsi sstatus, 2");
}

static void set_next_timer(void) {
    if (next_deadline == 0)
        next_deadline = rdtime64() + TIMER_INTERVAL;
    else
        next_deadline += TIMER_INTERVAL;

    sbi_set_timer(next_deadline);
}

void do_trap(struct pt_regs* regs) {
    unsigned long is_interrupt = regs->scause >> 63;
    unsigned long cause_code = regs->scause & 0xfff;

    if (is_interrupt && cause_code == 5) {
        boot_time_sec += INTERVAL_SEC;
        uart_puts("boot time: ");
        uart_put_dec(boot_time_sec);
        uart_puts("\n");
        set_next_timer();
        return;
    }

    uart_puts("unexpected trap, scause = ");
    uart_hex(regs->scause);
    uart_puts(", sepc = ");
    uart_hex(regs->sepc);
    uart_puts(", stval = ");
    uart_hex(regs->stval);
    uart_puts("\n");

    while (1) { }
}

void start_kernel(void) {
    uart_puts("\nStarting kernel ...\n");
    uart_puts("boot time: 0\n");

    enable_timer_interrupt();
    irq_enable();
    set_next_timer();

    while (1) {
        asm volatile("wfi");
    }
}