#include "types.h"
#include "mm.h"
#include "dtb.h"
#include "printk.h"

/* linker script 提供的 kernel 映像範圍 */
extern char __kernel_start[];
extern char __kernel_end[];

/* page metadata 狀態值 */
#define PAGE_FREE_BUDDY (-1)   /* buddy block 中的非首頁 page */
#define PAGE_RESERVED   (-2)   /* 保留頁，不可分配 */
#define CHUNK_MAGIC     0x43484b21u /* 用來辨識 chunk page 的 magic number */

/* 每個 order 對應一條 buddy free list */
struct free_area {
    struct list_head free_list;
};

/* 小區塊配置池，每種 chunk size 一個 pool */
struct chunk_pool {
    size_t chunk_size;          /* 這個 pool 管理的 chunk 大小 */
    struct list_head free_list; /* 空閒 chunk 串列 */
};

/* 每個 chunk 本身掛在 free list 上的節點 */
struct chunk_node {
    struct list_head node;
};

/* 放在 chunk page 開頭的 metadata */
struct chunk_page_meta {
    uint32_t magic;       /* 是否為 chunk page 的識別碼 */
    uint32_t chunk_size;  /* 這一頁切成的 chunk 大小 */
};

/* ===== 全域 allocator 狀態 ===== */

static struct boot_info *g_bi;         /* 開機資訊 */
static phys_addr_t managed_base;       /* 可管理實體記憶體起點 */
static size_t managed_pages;           /* 可管理總頁數 */
static struct page *mem_map;           /* 每個 page 對應的 metadata */
static struct free_area free_area[MAX_ORDER + 1]; /* 各 order 的 buddy free list */

/* chunk pools，支援不同大小的小區塊配置 */
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

/* 保留區表，記錄哪些記憶體不能分配 */
static struct mem_region boot_reserved[MAX_BOOT_RESERVED];
static int boot_reserved_cnt;

/* startup allocator 範圍 */
static phys_addr_t startup_cur; /* 下一次 startup_alloc 從哪裡開始找 */
static phys_addr_t startup_end; /* startup allocator 可使用結尾 */

/* ===== 基本工具函式 ===== */

/* 一般數值向上對齊到 a */
static size_t align_up(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

/* 實體位址向上對齊到 a */
static phys_addr_t p_align_up(phys_addr_t x, phys_addr_t a) {
    return (x + a - 1) & ~(a - 1);
}

/* 實體位址 -> page index */
static size_t page_idx_from_pa(phys_addr_t pa) {
    return (size_t)((pa - managed_base) >> PAGE_SHIFT);
}

/* page index -> 實體位址 */
static phys_addr_t page_pa(size_t idx) {
    return managed_base + ((phys_addr_t)idx << PAGE_SHIFT);
}

/* 判斷兩個區間 [a0, a1) 與 [b0, b1) 是否重疊 */
static int ranges_overlap(phys_addr_t a0, phys_addr_t a1,
                          phys_addr_t b0, phys_addr_t b1) {
    return !(a1 <= b0 || b1 <= a0);
}

/* ===== Reserved Memory 管理 ===== */

/* 將一段區域登記成保留區，之後不可被 allocator 使用 */
void reserve_region(phys_addr_t start, phys_addr_t size) {
    if (!size || boot_reserved_cnt >= MAX_BOOT_RESERVED)
        return;

    boot_reserved[boot_reserved_cnt].base = start;
    boot_reserved[boot_reserved_cnt].size = size;
    boot_reserved_cnt++;

    printk("[Reserve] [0x%lx, 0x%lx)\n", start, start + size);
}

/* 檢查一段區間是否與任何保留區重疊 */
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
 * 尋找一段可供 startup allocator 使用的範圍：
 * 1. 必須符合 alignment
 * 2. 不能與任何 reserved region 重疊
 */
static int find_free_range(phys_addr_t *out_start, size_t size, size_t align) {
    phys_addr_t cur = p_align_up(startup_cur, (phys_addr_t)align);

    while (cur + size <= startup_end) {
        phys_addr_t end = cur + size;
        int overlap = 0;

        for (int i = 0; i < boot_reserved_cnt; i++) {
            phys_addr_t rs = boot_reserved[i].base;
            phys_addr_t re = rs + boot_reserved[i].size;

            /* 若重疊，就把 cur 跳到該 reserved region 結尾之後再繼續找 */
            if (ranges_overlap(cur, end, rs, re)) {
                cur = p_align_up(re, (phys_addr_t)align);
                overlap = 1;
                break;
            }
        }

        /* 找到合法範圍 */
        if (!overlap) {
            *out_start = cur;
            return 0;
        }
    }

    return -1;
}

/* early boot allocator：只往前配置，不支援 free */
void *startup_alloc(size_t size, size_t align) {
    phys_addr_t cur;

    if (find_free_range(&cur, size, align) < 0)
        return NULL;

    startup_cur = cur + size;
    return (void *)(uintptr_t)cur;
}

/* ===== Buddy System ===== */

/* 將某個 block 的首頁 page 加入指定 order 的 free list */
static void add_to_free_list(size_t idx, unsigned int order) {
    struct page *pg = &mem_map[idx];
    pg->order = (int)order;
    pg->refcount = 0;
    INIT_LIST_HEAD(&pg->node);
    list_add_tail(&pg->node, &free_area[order].free_list);
}

/*
 * 根據 reserved memory 初始化 buddy system：
 * 1. 先把所有頁設成 reserved
 * 2. 掃描可管理區間
 * 3. 找出不和保留區重疊、且可對齊的最大合法 block
 * 4. 放入對應 order 的 free list
 */
static void buddy_init_from_reserved(void) {
    for (unsigned int i = 0; i <= MAX_ORDER; i++)
        INIT_LIST_HEAD(&free_area[i].free_list);

    /* 初始先假設所有 page 都不可分配 */
    for (size_t i = 0; i < managed_pages; i++) {
        mem_map[i].order = PAGE_RESERVED;
        mem_map[i].refcount = 1;
        INIT_LIST_HEAD(&mem_map[i].node);
    }

    size_t idx = 0;
    while (idx < managed_pages) {
        phys_addr_t start = page_pa(idx);

        /* 若這一頁本身就在 reserved range，就跳過 */
        if (is_reserved_range(start, start + PAGE_SIZE)) {
            idx++;
            continue;
        }

        /* 嘗試找從 idx 開始可放入的最大 order block */
        int order = MAX_ORDER;
        while (order > 0) {
            size_t block_pages = 1UL << order;

            /* 檢查 block 起點是否符合該 order 的對齊 */
            if (idx & (block_pages - 1)) {
                order--;
                continue;
            }

            /* 檢查 block 是否超出 managed range */
            if (idx + block_pages > managed_pages) {
                order--;
                continue;
            }

            /* 檢查整個 block 是否碰到 reserved range */
            if (is_reserved_range(start,
                                  start + ((phys_addr_t)block_pages << PAGE_SHIFT))) {
                order--;
                continue;
            }

            break;
        }

        /* 將這個 block 的首頁放入對應 free list */
        add_to_free_list(idx, (unsigned int)order);

        /* 其餘頁標記成 buddy block 的非首頁 */
        for (size_t j = 1; j < (1UL << order); j++) {
            mem_map[idx + j].order = PAGE_FREE_BUDDY;
            mem_map[idx + j].refcount = 0;
        }

        idx += (1UL << order);
    }
}

/* 配置 2^order pages */
struct page *alloc_pages(unsigned int order) {
    if (order > MAX_ORDER)
        return NULL;

    /* 從目標 order 開始找，找不到就往更高 order 找 */
    for (unsigned int cur = order; cur <= MAX_ORDER; cur++) {
        if (!list_empty(&free_area[cur].free_list)) {
            struct page *pg =
                list_first_entry(&free_area[cur].free_list, struct page, node);
            list_del(&pg->node);

            size_t pfn = (size_t)(pg - mem_map);

            /* 若拿到的 block 太大，就一路 split 到目標 order */
            while (cur > order) {
                cur--;

                size_t buddy_pfn = pfn + (1UL << cur);
                struct page *buddy = &mem_map[buddy_pfn];

                buddy->order = (int)cur;
                buddy->refcount = 0;
                INIT_LIST_HEAD(&buddy->node);
                list_add_tail(&buddy->node, &free_area[cur].free_list);

                /* split 出來的 buddy block 其餘頁設為非首頁 */
                for (size_t j = 1; j < (1UL << cur); j++) {
                    mem_map[buddy_pfn + j].order = PAGE_FREE_BUDDY;
                    mem_map[buddy_pfn + j].refcount = 0;
                }

                printk("[-] split to order %u, buddy idx=%lu\n", cur, buddy_pfn);
            }

            /* 最終回傳的 block 標成已配置 */
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

/* 釋放先前配置的 buddy block */
void free_pages(struct page *page) {
    if (!page || page->refcount == 0)
        return;

    size_t pfn = (size_t)(page - mem_map);
    unsigned int order = (unsigned int)page->order;

    page->refcount = 0;

    /* 先把 block 中非首頁頁標成 free buddy */
    for (size_t j = 1; j < (1UL << order); j++) {
        mem_map[pfn + j].order = PAGE_FREE_BUDDY;
        mem_map[pfn + j].refcount = 0;
    }

    /* 嘗試向上 merge */
    while (order < MAX_ORDER) {
        size_t buddy_pfn = pfn ^ (1UL << order);
        if (buddy_pfn >= managed_pages)
            break;

        struct page *buddy = &mem_map[buddy_pfn];

        /* buddy 必須是同 order 且目前 free，才能合併 */
        if (buddy->refcount != 0 || buddy->order != (int)order)
            break;

        list_del(&buddy->node);
        printk("[*] merge idx=%lu with buddy=%lu order=%u\n",
               pfn, buddy_pfn, order);

        /* 合併後首頁取較小的 pfn */
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

/* struct page -> 對應位址 */
void *page_to_virt(struct page *page) {
    return (void *)(uintptr_t)page_pa((size_t)(page - mem_map));
}

/* 位址 -> struct page */
struct page *virt_to_page(void *ptr) {
    phys_addr_t pa = (phys_addr_t)(uintptr_t)ptr;
    phys_addr_t limit = managed_base + ((phys_addr_t)managed_pages << PAGE_SHIFT);

    if (pa < managed_base || pa >= limit)
        return NULL;

    return &mem_map[page_idx_from_pa(pa & ~(PAGE_SIZE - 1))];
}

/* 依據 size 找到對應的 chunk pool */
static int pool_index(size_t size) {
    for (int i = 0; i < MAX_CHUNK_POOLS; i++) {
        if (size <= pools[i].chunk_size)
            return i;
    }
    return -1;
}

/* 當某個 pool 不夠用時，向 buddy allocator 要一頁並切成多個 chunks */
static void pool_grow(int idx) {
    struct page *pg = alloc_pages(0);
    if (!pg)
        return;

    char *base = (char *)page_to_virt(pg);
    struct chunk_page_meta *meta = (struct chunk_page_meta *)base;

    /* page 開頭寫入 metadata，供 kfree 辨識 */
    meta->magic = CHUNK_MAGIC;
    meta->chunk_size = (uint32_t)pools[idx].chunk_size;

    /* metadata 後面開始切成固定大小 chunks */
    size_t off = align_up(sizeof(struct chunk_page_meta), pools[idx].chunk_size);
    for (; off + pools[idx].chunk_size <= PAGE_SIZE; off += pools[idx].chunk_size) {
        struct chunk_node *node = (struct chunk_node *)(base + off);
        INIT_LIST_HEAD(&node->node);
        list_add_tail(&node->node, &pools[idx].free_list);
    }
}

/* 動態配置：
 * - size >= PAGE_SIZE：直接走 buddy allocator
 * - size < PAGE_SIZE：走 chunk pool
 */
void *kmalloc(size_t size) {
    if (size == 0)
        return NULL;

    /* 大配置：轉成頁數後向 buddy allocator 要 page block */
    if (size >= PAGE_SIZE) {
        unsigned int order = 0;
        size_t pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

        while ((1UL << order) < pages)
            order++;

        struct page *pg = alloc_pages(order);
        mm_dump(); // TA test
        return pg ? page_to_virt(pg) : NULL;
    }

    /* 小配置：找對應 pool */
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
    mm_dump(); //test
    return node;
}

/* 動態釋放：
 * - 若該位址所在 page 是 chunk page，就放回 pool
 * - 否則視為大配置，交給 free_pages()
 */
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
    mm_dump(); // test
}

/* 印出目前各 order 的 free block 數量 */
void mm_dump(void) {
    for (int i = MAX_ORDER; i >= 0; i--) {
        size_t cnt = 0;
        struct list_head *head = &free_area[i].free_list;
        for (struct list_head *p = head->next; p != head; p = p->next)
            cnt++;
        printk("free_area[%d] %lu\n", i, cnt);
    }
}

/* allocator 初始化入口 */
void mm_init(struct boot_info *bi) {
    g_bi = bi;

    /* 初始化可管理記憶體範圍 */
    boot_reserved_cnt = 0;
    managed_base = bi->mem.base;
    managed_pages = (size_t)(bi->mem.size >> PAGE_SHIFT);

    /* startup allocator 從整段 managed memory 開始 */
    startup_cur = managed_base;
    startup_end = managed_base + bi->mem.size;

    /* 保護 DTB */
    reserve_region((phys_addr_t)(uintptr_t)bi->dtb, bi->dtb_size);

    /* 保護 kernel image */
    reserve_region((phys_addr_t)(uintptr_t)__kernel_start,
                   (phys_addr_t)((uintptr_t)__kernel_end - (uintptr_t)__kernel_start));

    /* 若有 initrd，也保護起來 */
    if (bi->initrd_end > bi->initrd_start) {
        reserve_region(bi->initrd_start, bi->initrd_end - bi->initrd_start);
    }

    /* 從 device tree 取得額外 reserved regions */
    struct mem_region extra[16];
    int nr = dtb_get_reserved_regions(bi->dtb, extra, 16);
    for (int i = 0; i < nr; i++) {
        reserve_region(extra[i].base, extra[i].size);
    }

    /* 先用 startup allocator 配置 mem_map */
    mem_map = (struct page *)startup_alloc(managed_pages * sizeof(struct page), PAGE_SIZE);
    if (!mem_map) {
        printk("[MM] startup_alloc for mem_map failed\n");
        while (1)
            asm volatile("wfi");
    }

    /* mem_map 自己也要列為保留區，避免被 allocator 分出去 */
    reserve_region((phys_addr_t)(uintptr_t)mem_map,
                   managed_pages * sizeof(struct page));

    /* 初始化所有 chunk pools */
    for (int i = 0; i < MAX_CHUNK_POOLS; i++) {
        INIT_LIST_HEAD(&pools[i].free_list);
    }

    /* 根據 reserved memory 建立 buddy free lists */
    buddy_init_from_reserved();

    printk("[MM] base=0x%lx size=0x%lx pages=%lu mem_map=%p boot_info=%p\n",
           bi->mem.base, bi->mem.size, managed_pages, mem_map, g_bi);
}