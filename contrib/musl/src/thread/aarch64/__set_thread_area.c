#include <elf.h>
#include "libc.h"

#define BITRANGE(a,b) (2*(1UL<<(b))-(1UL<<(a)))

int __set_thread_area(void *p)
{
	__asm__ __volatile__ ("msr tpidr_el0,%0" : : "r"(p) : "memory");

	/* Mask off hwcap bits for SME and unknown future features. This is
	 * necessary because SME is not safe to use without libc support for
	 * it, and we do not (yet) have such support. */
	for (size_t *v = libc.auxv; *v; v+=2) {
		if (v[0]==AT_HWCAP) {
			v[1] &= ~BITRANGE(42,63); /* 42-47 are SME */
		} else if (v[0]==AT_HWCAP2) {
			v[1] &= ~(BITRANGE(23,30)
			        | BITRANGE(37,42)
			        | BITRANGE(57,62));
		} else if (v[0]==AT_HWCAP3 || v[0]==AT_HWCAP4) {
			v[0] = AT_IGNORE;
			v[1] = 0;
		}
	}

	return 0;
}
