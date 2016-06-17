#ifndef __ASM_SH_DIV64
#define __ASM_SH_DIV64

extern u64 __xdiv64_32(u64 n, u32 d);

#define do_div(n,base) ({ \
u64 __n = (n), __q; \
u32 __base = (base); \
u32 __res; \
if ((__n >> 32) == 0) { \
	__res = ((unsigned long) __n) % (unsigned) __base; \
	(n) = ((unsigned long) __n) / (unsigned) __base; \
} else { \
	__q = __xdiv64_32(__n, __base); \
	__res = __n - __q * __base; \
	(n) = __q; \
} \
__res; })

#endif /* __ASM_SH_DIV64 */
