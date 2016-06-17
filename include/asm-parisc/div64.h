#ifndef __ASM_PARISC_DIV64
#define __ASM_PARISC_DIV64

#ifdef __LP64__

/*
 * Copyright (C) 1999 Hewlett-Packard Co
 * Copyright (C) 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * vsprintf uses this to divide a 64-bit integer N by a small integer BASE.
 * This is incredibly hard on IA-64 and HPPA
 */

#define do_div(n,base)						\
({								\
	int _res;						\
	_res = ((unsigned long) (n)) % (unsigned) (base);	\
	(n) = ((unsigned long) (n)) / (unsigned) (base);	\
	_res;							\
})

#else
/*
 * unsigned long long division.  Yuck Yuck!  What is Linux coming to?
 * This is 100% disgusting
 */
#define do_div(n,base)							\
({									\
	unsigned long __low, __low2, __high, __rem;			\
	__low  = (n) & 0xffffffff;					\
	__high = (n) >> 32;						\
	if (__high) {							\
		__rem   = __high % (unsigned long)base;			\
		__high  = __high / (unsigned long)base;			\
		__low2  = __low >> 16;					\
		__low2 += __rem << 16;					\
		__rem   = __low2 % (unsigned long)base;			\
		__low2  = __low2 / (unsigned long)base;			\
		__low   = __low & 0xffff;				\
		__low  += __rem << 16;					\
		__rem   = __low  % (unsigned long)base;			\
		__low   = __low  / (unsigned long)base;			\
		n = __low  + ((long long)__low2 << 16) +		\
			((long long) __high << 32);			\
	} else {							\
		__rem = __low % (unsigned long)base;			\
		n = (__low / (unsigned long)base);			\
	}								\
	__rem;								\
})
#endif

#endif

