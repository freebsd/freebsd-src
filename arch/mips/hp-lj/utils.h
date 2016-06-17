/*
 *
 *
 *
 */

#include <linux/types.h>

#define ReservedMemVirtualAddr  0x50000000

unsigned long get_mem_avail(void);

ulong* get_reserved_buffer(void);
ulong* get_reserved_buffer_virtual(void);
ulong get_reserved_buffer_size(void);

void  reserve_buffer(const char* cl, ulong base_mem);


