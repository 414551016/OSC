#include "types.h"
#include "dtb.h"

#define FDT_MAGIC      0xd00dfeedU
#define FDT_BEGIN_NODE 0x00000001U
#define FDT_END_NODE   0x00000002U
#define FDT_PROP       0x00000003U
#define FDT_NOP        0x00000004U
#define FDT_END        0x00000009U

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
};

static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static uint32_t be32_to_cpu(uint32_t x) {
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

static uint64_t __attribute__((unused)) be64_to_cpu(uint64_t x) {
    return ((uint64_t)be32_to_cpu((uint32_t)(x >> 32))) |
           ((uint64_t)be32_to_cpu((uint32_t)x) << 32);
}

static uint32_t align4(uint32_t x) {
    return (x + 3U) & ~3U;
}

static const char *fdt_strings(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return (const char *)fdt + be32_to_cpu(h->off_dt_strings);
}

static const uint32_t *fdt_struct(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return (const uint32_t *)((const char *)fdt + be32_to_cpu(h->off_dt_struct));
}

static uint32_t fdt_struct_size(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return be32_to_cpu(h->size_dt_struct);
}

static int fdt_check_header(const void *fdt) {
    const struct fdt_header *h = (const struct fdt_header *)fdt;
    return be32_to_cpu(h->magic) == FDT_MAGIC ? 0 : -1;
}

static uint64_t fdt_reg_value(const void *reg, int cells) {
    const uint32_t *r = (const uint32_t *)reg;
    if (cells == 2)
        return ((uint64_t)be32_to_cpu(r[0]) << 32) | be32_to_cpu(r[1]);
    return be32_to_cpu(r[0]);
}

static const void *fdt_getprop_by_path(const void *fdt,
                                       const char *target_path,
                                       const char *prop_name,
                                       int *lenp) {
    const uint32_t *p = fdt_struct(fdt);
    const char *strs = fdt_strings(fdt);
    const char *end = (const char *)p + fdt_struct_size(fdt);
    char path[256];
    int depth = 0;

    path[0] = '\0';

    while ((const char *)p < end) {
        uint32_t token = be32_to_cpu(*p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            size_t nlen = kstrlen(name);

            if (depth == 0) {
                path[0] = '/';
                path[1] = '\0';
            } else {
                size_t plen = kstrlen(path);
                if (plen > 1) path[plen++] = '/';
                for (size_t i = 0; i < nlen; i++) path[plen + i] = name[i];
                path[plen + nlen] = '\0';
            }

            depth++;
            p = (const uint32_t *)((const char *)p + align4((uint32_t)nlen + 1));
        } else if (token == FDT_END_NODE) {
            depth--;
            if (depth <= 0) {
                path[0] = '\0';
                depth = 0;
            } else {
                size_t plen = kstrlen(path);
                while (plen > 1 && path[plen - 1] != '/') plen--;
                if (plen > 1) path[plen - 1] = '\0';
                else {
                    path[0] = '/';
                    path[1] = '\0';
                }
            }
        } else if (token == FDT_PROP) {
            const struct fdt_prop *prop = (const struct fdt_prop *)p;
            uint32_t len = be32_to_cpu(prop->len);
            uint32_t nameoff = be32_to_cpu(prop->nameoff);
            const char *name = strs + nameoff;
            const void *value = (const char *)(prop + 1);

            if (kstrcmp(path, target_path) == 0 && kstrcmp(name, prop_name) == 0) {
                if (lenp) *lenp = (int)len;
                return value;
            }

            p = (const uint32_t *)((const char *)(prop + 1) + align4(len));
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            return NULL;
        }
    }

    return NULL;
}

void dtb_init(void *dtb, struct boot_info *bi) {
    int len = 0;
    const struct fdt_header *hdr = (const struct fdt_header *)dtb;
    const void *reg;

    bi->dtb = dtb;
    bi->dtb_size = be32_to_cpu(hdr->totalsize);
    bi->initrd_start = 0;
    bi->initrd_end = 0;
    bi->mem.base = 0;
    bi->mem.size = 0;

    if (fdt_check_header(dtb) < 0) return;

    reg = fdt_getprop_by_path(dtb, "/memory", "reg", &len);
    if (!reg) reg = fdt_getprop_by_path(dtb, "/memory@0", "reg", &len);

    if (reg && len >= 16) {
        bi->mem.base = fdt_reg_value(reg, 2);
        bi->mem.size = fdt_reg_value((const char *)reg + 8, 2);
    } else if (reg && len >= 8) {
        bi->mem.base = fdt_reg_value(reg, 1);
        bi->mem.size = fdt_reg_value((const char *)reg + 4, 1);
    }

    reg = fdt_getprop_by_path(dtb, "/chosen", "linux,initrd-start", &len);
    if (reg) bi->initrd_start = (len >= 8) ? fdt_reg_value(reg, 2) : fdt_reg_value(reg, 1);

    reg = fdt_getprop_by_path(dtb, "/chosen", "linux,initrd-end", &len);
    if (reg) bi->initrd_end = (len >= 8) ? fdt_reg_value(reg, 2) : fdt_reg_value(reg, 1);
}

int dtb_get_reserved_regions(void *dtb, struct mem_region *out, int max_regions) {
    const uint32_t *p = fdt_struct(dtb);
    const char *strs = fdt_strings(dtb);
    const char *end = (const char *)p + fdt_struct_size(dtb);
    char path[256];
    int depth = 0;
    int count = 0;

    path[0] = '\0';

    while ((const char *)p < end) {
        uint32_t token = be32_to_cpu(*p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            size_t nlen = kstrlen(name);

            if (depth == 0) {
                path[0] = '/';
                path[1] = '\0';
            } else {
                size_t plen = kstrlen(path);
                if (plen > 1) path[plen++] = '/';
                for (size_t i = 0; i < nlen; i++) path[plen + i] = name[i];
                path[plen + nlen] = '\0';
            }

            depth++;
            p = (const uint32_t *)((const char *)p + align4((uint32_t)nlen + 1));
        } else if (token == FDT_END_NODE) {
            depth--;
            if (depth <= 0) {
                path[0] = '\0';
                depth = 0;
            } else {
                size_t plen = kstrlen(path);
                while (plen > 1 && path[plen - 1] != '/') plen--;
                if (plen > 1) path[plen - 1] = '\0';
                else {
                    path[0] = '/';
                    path[1] = '\0';
                }
            }
        } else if (token == FDT_PROP) {
            const struct fdt_prop *prop = (const struct fdt_prop *)p;
            uint32_t len = be32_to_cpu(prop->len);
            uint32_t nameoff = be32_to_cpu(prop->nameoff);
            const char *name = strs + nameoff;
            const void *value = (const char *)(prop + 1);

            if (count < max_regions && name && kstrcmp(name, "reg") == 0) {
                if (kstrcmp(path, "/reserved-memory") != 0) {
                    const char *prefix = "/reserved-memory/";
                    int ok = 1;
                    for (int i = 0; prefix[i]; i++) {
                        if (path[i] != prefix[i]) {
                            ok = 0;
                            break;
                        }
                    }
                    if (ok && len >= 16) {
                        out[count].base = fdt_reg_value(value, 2);
                        out[count].size = fdt_reg_value((const char *)value + 8, 2);
                        count++;
                    }
                }
            }

            p = (const uint32_t *)((const char *)(prop + 1) + align4(len));
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            break;
        }
    }

    return count;
}