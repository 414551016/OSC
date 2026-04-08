#ifndef COMMON_H
#define COMMON_H

/* 基本型別定義 */
typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef long           s64;
typedef int            s32;

typedef unsigned long size_t;
typedef long          ssize_t;

/* NULL 定義 */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* 頁大小：Lab3 後續 buddy allocator 會用到 */
#define PAGE_SIZE 4096UL

/* 對齊工具 */
#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

/* 簡單記憶體操作，避免依賴 libc */
static inline void *memset_simple(void *dst, int value, size_t n)
{
    u8 *p = (u8 *)dst;
    for (size_t i = 0; i < n; i++) {
        p[i] = (u8)value;
    }
    return dst;
}

static inline void *memcpy_simple(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

#endif