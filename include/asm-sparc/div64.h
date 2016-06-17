#ifndef __SPARC_DIV64
#define __SPARC_DIV64

/* We're not 64-bit, but... */
#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) n) % (unsigned) base; \
	n = ((unsigned long) n) / (unsigned) base; \
	__res; })

#endif /* __SPARC_DIV64 */
