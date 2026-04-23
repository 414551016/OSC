#ifndef DTB_H
#define DTB_H

#include "types.h"

void dtb_init(void *dtb, struct boot_info *bi);
int dtb_get_reserved_regions(void *dtb, struct mem_region *out, int max_regions);

#endif