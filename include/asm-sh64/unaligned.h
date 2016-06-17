#ifndef __ASM_SH64_UNALIGNED_H
#define __ASM_SH64_UNALIGNED_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/unaligned.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 * SH can't handle unaligned accesses.
 *
 */

#include <linux/string.h>


/* Use memmove here, so gcc does not insert a __builtin_memcpy. */

#define get_unaligned(ptr) \
  ({ __typeof__(*(ptr)) __tmp; memmove(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#define put_unaligned(val, ptr)				\
  ({ __typeof__(*(ptr)) __tmp = (val);			\
     memmove((ptr), &__tmp, sizeof(*(ptr)));		\
     (void)0; })

#endif /* __ASM_SH64_UNALIGNED_H */
