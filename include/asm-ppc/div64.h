#ifndef __PPC_DIV64
#define __PPC_DIV64

#include <linux/types.h>

extern u32 __div64_32(u64 *dividend, u32 div);

#define do_div(n, div)	({			\
	u64 __n = (n);				\
	u32 __d = (div);			\
	u32 __q, __r;				\
	if ((__n >> 32) == 0) {			\
		__q = (u32)__n / __d;		\
		__r = (u32)__n - __q * __d;	\
		(n) = __q;			\
	} else {				\
		__r = __div64_32(&__n, __d);	\
		(n) = __n;			\
	}					\
	__r;					\
})

#endif
