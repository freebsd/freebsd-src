#ifndef __SPARC64_DIV64
#define __SPARC64_DIV64

/*
 * Hey, we're already 64-bit, no
 * need to play games..
 */
#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) n) % (unsigned) base; \
	n = ((unsigned long) n) / (unsigned) base; \
	__res; })

#endif /* __SPARC64_DIV64 */
