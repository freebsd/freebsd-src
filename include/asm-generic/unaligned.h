#ifndef _ASM_GENERIC_UNALIGNED_H_
#define _ASM_GENERIC_UNALIGNED_H_

/*
 * For the benefit of those who are trying to port Linux to another
 * architecture, here are some C-language equivalents. 
 */

#include <asm/string.h>


#define get_unaligned(ptr) \
  ({ __typeof__(*(ptr)) __tmp; memcpy(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#define put_unaligned(val, ptr)				\
  ({ __typeof__(*(ptr)) __tmp = (val);			\
     memcpy((ptr), &__tmp, sizeof(*(ptr)));		\
     (void)0; })

#endif /* _ASM_GENERIC_UNALIGNED_H */
