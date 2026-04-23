#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t phys_addr_t;

struct mem_region {
    phys_addr_t base;
    phys_addr_t size;
};

struct boot_info {
    void *dtb;
    phys_addr_t dtb_size;

    struct mem_region mem;

    phys_addr_t initrd_start;
    phys_addr_t initrd_end;
};

#endif