#ifndef __ASM_ARM_DIV64
#define __ASM_ARM_DIV64

/* We're not 64-bit, but... */
#define do_div(n,base)						\
({								\
	int __res;						\
	__res = ((unsigned long)n) % (unsigned int)base;	\
	n = ((unsigned long)n) / (unsigned int)base;		\
	__res;							\
})

#endif

