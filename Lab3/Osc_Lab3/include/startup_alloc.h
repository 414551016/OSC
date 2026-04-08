#ifndef STARTUP_ALLOC_H
#define STARTUP_ALLOC_H

#include "common.h"

/* 初始化 startup allocator */
void startup_alloc_init(void *heap_start, void *heap_end);

/* 配置記憶體（只往前切，不回收） */
void *startup_alloc(size_t size, size_t align);

/* 取得目前 allocator 狀態 */
u64 startup_alloc_used(void);
u64 startup_alloc_remaining(void);

#endif