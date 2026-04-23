#ifndef MM_H
#define MM_H

#include "types.h"
#include "list.h"

#define PAGE_SHIFT 12UL
#define PAGE_SIZE  (1UL << PAGE_SHIFT)

#define MAX_ORDER 10
#define MAX_CHUNK_POOLS 8
#define MAX_BOOT_RESERVED 32

struct page {
    int order;
    int refcount;
    struct list_head node;
};

void mm_init(struct boot_info *bi);
void mm_dump(void);

void *startup_alloc(size_t size, size_t align);
void reserve_region(phys_addr_t start, phys_addr_t size);

struct page *alloc_pages(unsigned int order);
void free_pages(struct page *page);

void *page_to_virt(struct page *page);
struct page *virt_to_page(void *ptr);

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif