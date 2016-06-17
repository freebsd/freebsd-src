#ifndef _CRIS_BYTEORDER_H
#define _CRIS_BYTEORDER_H

#include <asm/types.h>

#ifdef __GNUC__

/* we just define these two (as we can do the swap in a single
 * asm instruction in CRIS) and the arch-independent files will put
 * them together into ntohl etc.
 */

extern __inline__ __const__ __u32 ___arch__swab32(__u32 x)
{
	__asm__ ("swapwb %0" : "=r" (x) : "0" (x));

	return(x);
}

extern __inline__ __const__ __u16 ___arch__swab16(__u16 x)
{
	__asm__ ("swapb %0" : "=r" (x) : "0" (x));

	return(x);
}

/* defines are necessary because the other files detect the presence
 * of a defined __arch_swab32, not an inline
 */

#define __arch__swab32(x) ___arch__swab32(x)
#define __arch__swab16(x) ___arch__swab16(x)

#if !defined(__STRICT_ANSI__) || defined(__KERNEL__)
#  define __BYTEORDER_HAS_U64__
#  define __SWAB_64_THRU_32__
#endif

#endif /* __GNUC__ */

#include <linux/byteorder/little_endian.h>

#endif
