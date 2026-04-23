#include "types.h"
#include "mm.h"
#include "dtb.h"
#include "printk.h"

extern char __kernel_start[];
extern char __kernel_end[];

#define PAGE_FREE_BUDDY (-1)
#define PAGE_RESERVED   (-2)
#define CHUNK_MAGIC     0x43484b21u

struct free_area {
    struct list_head free_list;
};

struct chunk_pool {
    size_t chunk_size;
    struct list_head free_list;
};

struct chunk_node {
    struct list_head node;
};

struct chunk_page_meta {
    uint32_t magic;
    uint32_t chunk_size;
};

static struct boot_info *g_bi;
static phys_addr_t managed_base;
static size_t managed_pages;
static struct page *mem_map;
static struct free_area free_area[MAX_ORDER + 1];

static struct chunk_pool pools[MAX_CHUNK_POOLS] = {
    { .chunk_size = 16 },
    { .chunk_size = 32 },
    { .chunk_size = 64 },
    { .chunk_size = 128 },
    { .chunk_size = 256 },
    { .chunk_size = 512 },
    { .chunk_size = 1024 },
    { .chunk_size = 2048 },
};

static struct mem_region boot_reserved[MAX_BOOT_RESERVED];
static int boot_reserved_cnt;
static phys_addr_t startup_cur;
static phys_addr_t startup_end;

static size_t align_up(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

static phys_addr_t p_align_up(phys_addr_t x, phys_addr_t a) {
    return (x + a - 1) & ~(a - 1);
}

static size_t page_idx_from_pa(phys_addr_t pa) {
    return (size_t)((pa - managed_base) >> PAGE_SHIFT);
}

static phys_addr_t page_pa(size_t idx) {
    return managed_base + ((phys_addr_t)idx << PAGE_SHIFT);
}

static int ranges_overlap(phys_addr_t a0, phys_addr_t a1,
                          phys_addr_t b0, phys_addr_t b1) {
    return !(a1 <= b0 || b1 <= a0);
}

void reserve_region(phys_addr_t start, phys_addr_t size) {
    if (!size || boot_reserved_cnt >= MAX_BOOT_RESERVED)
        return;

    boot_reserved[boot_reserved_cnt].base = start;
    boot_reserved[boot_reserved_cnt].size = size;
    boot_reserved_cnt++;

    printk("[Reserve] [0x%lx, 0x%lx)\n", start, start + size);
}

static int is_reserved_range(phys_addr_t start, phys_addr_t end) {
    for (int i = 0; i < boot_reserved_cnt; i++) {
        phys_addr_t rs = boot_reserved[i].base;
        phys_addr_t re = rs + boot_reserved[i].size;
        if (ranges_overlap(start, end, rs, re))
            return 1;
    }
    return 0;
}

/*
 * Find a usable startup allocation range that:
 * 1. satisfies alignment
 * 2. does not overlap any reserved region
 */
static int find_free_range(phys_addr_t *out_start, size_t size, size_t align) {
    phys_addr_t cur = p_align_up(startup_cur, (phys_addr_t)align);

    while (cur + size <= startup_end) {
        phys_addr_t end = cur + size;
        int overlap = 0;

        for (int i = 0; i < boot_reserved_cnt; i++) {
            phys_addr_t rs = boot_reserved[i].base;
            phys_addr_t re = rs + boot_reserved[i].size;

            if (ranges_overlap(cur, end, rs, re)) {
                cur = p_align_up(re, (phys_addr_t)align);
                overlap = 1;
                break;
            }
        }

        if (!overlap) {
            *out_start = cur;
            return 0;
        }
    }

    return -1;
}

void *startup_alloc(size_t size, size_t align) {
    phys_addr_t cur;

    if (find_free_range(&cur, size, align) < 0)
        return NULL;

    startup_cur = cur + size;
    return (void *)(uintptr_t)cur;
}

static void add_to_free_list(size_t idx, unsigned int order) {
    struct page *pg = &mem_map[idx];
    pg->order = (int)order;
    pg->refcount = 0;
    INIT_LIST_HEAD(&pg->node);
    list_add_tail(&pg->node, &free_area[order].free_list);
}

static void buddy_init_from_reserved(void) {
    for (unsigned int i = 0; i <= MAX_ORDER; i++)
        INIT_LIST_HEAD(&free_area[i].free_list);

    for (size_t i = 0; i < managed_pages; i++) {
        mem_map[i].order = PAGE_RESERVED;
        mem_map[i].refcount = 1;
        INIT_LIST_HEAD(&mem_map[i].node);
    }

    size_t idx = 0;
    while (idx < managed_pages) {
        phys_addr_t start = page_pa(idx);

        if (is_reserved_range(start, start + PAGE_SIZE)) {
            idx++;
            continue;
        }

        int order = MAX_ORDER;
        while (order > 0) {
            size_t block_pages = 1UL << order;

            if (idx & (block_pages - 1)) {
                order--;
                continue;
            }

            if (idx + block_pages > managed_pages) {
                order--;
                continue;
            }

            if (is_reserved_range(start,
                                  start + ((phys_addr_t)block_pages << PAGE_SHIFT))) {
                order--;
                continue;
            }

            break;
        }

        add_to_free_list(idx, (unsigned int)order);

        for (size_t j = 1; j < (1UL << order); j++) {
            mem_map[idx + j].order = PAGE_FREE_BUDDY;
            mem_map[idx + j].refcount = 0;
        }

        idx += (1UL << order);
    }
}

struct page *alloc_pages(unsigned int order) {
    if (order > MAX_ORDER)
        return NULL;

    for (unsigned int cur = order; cur <= MAX_ORDER; cur++) {
        if (!list_empty(&free_area[cur].free_list)) {
            struct page *pg =
                list_first_entry(&free_area[cur].free_list, struct page, node);
            list_del(&pg->node);

            size_t pfn = (size_t)(pg - mem_map);

            while (cur > order) {
                cur--;

                size_t buddy_pfn = pfn + (1UL << cur);
                struct page *buddy = &mem_map[buddy_pfn];

                buddy->order = (int)cur;
                buddy->refcount = 0;
                INIT_LIST_HEAD(&buddy->node);
                list_add_tail(&buddy->node, &free_area[cur].free_list);

                for (size_t j = 1; j < (1UL << cur); j++) {
                    mem_map[buddy_pfn + j].order = PAGE_FREE_BUDDY;
                    mem_map[buddy_pfn + j].refcount = 0;
                }

                printk("[-] split to order %u, buddy idx=%lu\n", cur, buddy_pfn);
            }

            pg->order = (int)order;
            pg->refcount = 1;

            for (size_t j = 1; j < (1UL << order); j++) {
                mem_map[pfn + j].order = PAGE_RESERVED;
                mem_map[pfn + j].refcount = 1;
            }

            printk("[Page] alloc idx=%lu order=%u pa=0x%lx\n",
                   pfn, order, page_pa(pfn));
            return pg;
        }
    }

    return NULL;
}

void free_pages(struct page *page) {
    if (!page || page->refcount == 0)
        return;

    size_t pfn = (size_t)(page - mem_map);
    unsigned int order = (unsigned int)page->order;

    page->refcount = 0;

    for (size_t j = 1; j < (1UL << order); j++) {
        mem_map[pfn + j].order = PAGE_FREE_BUDDY;
        mem_map[pfn + j].refcount = 0;
    }

    while (order < MAX_ORDER) {
        size_t buddy_pfn = pfn ^ (1UL << order);
        if (buddy_pfn >= managed_pages)
            break;

        struct page *buddy = &mem_map[buddy_pfn];
        if (buddy->refcount != 0 || buddy->order != (int)order)
            break;

        list_del(&buddy->node);
        printk("[*] merge idx=%lu with buddy=%lu order=%u\n",
               pfn, buddy_pfn, order);

        if (buddy_pfn < pfn) {
            page = buddy;
            pfn = buddy_pfn;
        }

        order++;
        page->order = (int)order;

        for (size_t j = 1; j < (1UL << order); j++) {
            mem_map[pfn + j].order = PAGE_FREE_BUDDY;
            mem_map[pfn + j].refcount = 0;
        }
    }

    INIT_LIST_HEAD(&page->node);
    list_add_tail(&page->node, &free_area[order].free_list);
    printk("[Page] free idx=%lu order=%u pa=0x%lx\n",
           pfn, order, page_pa(pfn));
}

void *page_to_virt(struct page *page) {
    return (void *)(uintptr_t)page_pa((size_t)(page - mem_map));
}

struct page *virt_to_page(void *ptr) {
    phys_addr_t pa = (phys_addr_t)(uintptr_t)ptr;
    phys_addr_t limit = managed_base + ((phys_addr_t)managed_pages << PAGE_SHIFT);

    if (pa < managed_base || pa >= limit)
        return NULL;

    return &mem_map[page_idx_from_pa(pa & ~(PAGE_SIZE - 1))];
}

static int pool_index(size_t size) {
    for (int i = 0; i < MAX_CHUNK_POOLS; i++) {
        if (size <= pools[i].chunk_size)
            return i;
    }
    return -1;
}

static void pool_grow(int idx) {
    struct page *pg = alloc_pages(0);
    if (!pg)
        return;

    char *base = (char *)page_to_virt(pg);
    struct chunk_page_meta *meta = (struct chunk_page_meta *)base;

    meta->magic = CHUNK_MAGIC;
    meta->chunk_size = (uint32_t)pools[idx].chunk_size;

    size_t off = align_up(sizeof(struct chunk_page_meta), pools[idx].chunk_size);
    for (; off + pools[idx].chunk_size <= PAGE_SIZE; off += pools[idx].chunk_size) {
        struct chunk_node *node = (struct chunk_node *)(base + off);
        INIT_LIST_HEAD(&node->node);
        list_add_tail(&node->node, &pools[idx].free_list);
    }
}

void *kmalloc(size_t size) {
    if (size == 0)
        return NULL;

    if (size >= PAGE_SIZE) {
        unsigned int order = 0;
        size_t pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

        while ((1UL << order) < pages)
            order++;

        struct page *pg = alloc_pages(order);
        return pg ? page_to_virt(pg) : NULL;
    }

    int idx = pool_index(size);
    if (idx < 0)
        return NULL;

    if (list_empty(&pools[idx].free_list))
        pool_grow(idx);
    if (list_empty(&pools[idx].free_list))
        return NULL;

    struct chunk_node *node =
        list_first_entry(&pools[idx].free_list, struct chunk_node, node);
    list_del(&node->node);

    printk("[Chunk] alloc %p size=%lu\n", node, pools[idx].chunk_size);
    return node;
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    phys_addr_t pa = (phys_addr_t)(uintptr_t)ptr;
    phys_addr_t page_base = pa & ~(PAGE_SIZE - 1);
    phys_addr_t limit = managed_base + ((phys_addr_t)managed_pages << PAGE_SHIFT);

    if (page_base >= managed_base && page_base < limit) {
        struct chunk_page_meta *meta = (struct chunk_page_meta *)(uintptr_t)page_base;
        if (meta->magic == CHUNK_MAGIC) {
            int idx = pool_index(meta->chunk_size);
            if (idx >= 0) {
                struct chunk_node *node = (struct chunk_node *)ptr;
                INIT_LIST_HEAD(&node->node);
                list_add_tail(&node->node, &pools[idx].free_list);
                printk("[Chunk] free %p size=%u\n", ptr, meta->chunk_size);
                return;
            }
        }
    }

    free_pages(virt_to_page(ptr));
}

void mm_dump(void) {
    for (int i = MAX_ORDER; i >= 0; i--) {
        size_t cnt = 0;
        struct list_head *head = &free_area[i].free_list;
        for (struct list_head *p = head->next; p != head; p = p->next)
            cnt++;
        printk("free_area[%d] %lu\n", i, cnt);
    }
}

void mm_init(struct boot_info *bi) {
    g_bi = bi;

    boot_reserved_cnt = 0;
    managed_base = bi->mem.base;
    managed_pages = (size_t)(bi->mem.size >> PAGE_SHIFT);

    startup_cur = managed_base;
    startup_end = managed_base + bi->mem.size;

    reserve_region((phys_addr_t)(uintptr_t)bi->dtb, bi->dtb_size);

    reserve_region((phys_addr_t)(uintptr_t)__kernel_start,
                   (phys_addr_t)((uintptr_t)__kernel_end - (uintptr_t)__kernel_start));

    if (bi->initrd_end > bi->initrd_start) {
        reserve_region(bi->initrd_start, bi->initrd_end - bi->initrd_start);
    }

    struct mem_region extra[16];
    int nr = dtb_get_reserved_regions(bi->dtb, extra, 16);
    for (int i = 0; i < nr; i++) {
        reserve_region(extra[i].base, extra[i].size);
    }

    mem_map = (struct page *)startup_alloc(managed_pages * sizeof(struct page), PAGE_SIZE);
    if (!mem_map) {
        printk("[MM] startup_alloc for mem_map failed\n");
        while (1)
            asm volatile("wfi");
    }

    reserve_region((phys_addr_t)(uintptr_t)mem_map,
                   managed_pages * sizeof(struct page));

    for (int i = 0; i < MAX_CHUNK_POOLS; i++) {
        INIT_LIST_HEAD(&pools[i].free_list);
    }

    buddy_init_from_reserved();

    printk("[MM] base=0x%lx size=0x%lx pages=%lu mem_map=%p boot_info=%p\n",
           bi->mem.base, bi->mem.size, managed_pages, mem_map, g_bi);
}