#include "common.h"
#include "startup_alloc.h"

/* =========================================================
 * startup allocator:
 * - 開機初期使用
 * - bump pointer / linear allocator
 * - 只會一直往前分配，不支援 free
 * =========================================================
 */

static u64 g_heap_cur  = 0;
static u64 g_heap_end  = 0;
static u64 g_heap_base = 0;

void startup_alloc_init(void *heap_start, void *heap_end)
{
    g_heap_base = (u64)heap_start;
    g_heap_cur  = (u64)heap_start;
    g_heap_end  = (u64)heap_end;
}

void *startup_alloc(size_t size, size_t align)
{
    if (align == 0) {
        align = 1;
    }

    u64 cur = ALIGN_UP(g_heap_cur, align);
    u64 next = cur + size;

    if (next > g_heap_end) {
        return NULL;
    }

    g_heap_cur = next;
    return (void *)cur;
}

u64 startup_alloc_used(void)
{
    return g_heap_cur - g_heap_base;
}

u64 startup_alloc_remaining(void)
{
    return g_heap_end - g_heap_cur;
}