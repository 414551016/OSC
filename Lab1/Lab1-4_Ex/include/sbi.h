#ifndef SBI_H
#define SBI_H

struct sbiret {
    long error;
    long value;
};

#define SBI_EXT_BASE                    0x10
#define SBI_EXT_BASE_GET_SPEC_VERSION   0
#define SBI_EXT_BASE_GET_IMPL_ID        1
#define SBI_EXT_BASE_GET_IMPL_VERSION   2

struct sbiret sbi_ecall(int ext, int fid,
                        unsigned long arg0, unsigned long arg1,
                        unsigned long arg2, unsigned long arg3,
                        unsigned long arg4, unsigned long arg5);

unsigned long sbi_get_spec_version(void);
unsigned long sbi_get_impl_id(void);
unsigned long sbi_get_impl_version(void);

#endif