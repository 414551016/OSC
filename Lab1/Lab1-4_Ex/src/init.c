extern char _bss_start[];
extern char _bss_end[];

void clear_bss(void)
{
    char *p = _bss_start;

    while (p < _bss_end) {
        *p++ = 0;
    }
}