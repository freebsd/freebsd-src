/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 */

#ifndef _MACHINE_CLOCK_H_
#define _MACHINE_CLOCK_H_ 1

void inittodr(time_t base);

extern int pentium_mhz;

#ifdef I586_CPU
	/*
	 * This resets the CPU cycle counter to zero, to make our
	 * job easier in microtime().  Some fancy ifdefs could speed
	 * this up for Pentium-only kernels.
	 * We want this to be done as close as possible to the actual
	 * timer incrementing in hardclock(), because there is a window
	 * between the two where the value is no longer valid.  Experimentation
	 * may reveal a good precompensation to apply in microtime().
	 */
#define CPU_CLOCKUPDATE(otime, ntime) \
	do { \
	if(pentium_mhz) { \
		__asm __volatile("cli\n" \
				 "movl (%2),%%eax\n" \
				 "movl %%eax,(%1)\n" \
				 "movl 4(%2),%%eax\n" \
				 "movl %%eax,4(%1)\n" \
				 "movl $0x10,%%ecx\n" \
				 "xorl %%eax,%%eax\n" \
				 "movl %%eax,%%edx\n" \
				 ".byte 0x0f, 0x30\n" \
				 "sti\n" \
				 "#%0%1%2" \
				 : "=m"(*otime)	/* no outputs */ \
				 : "c"(otime), "b"(ntime) /* fake input */ \
				 : "ax", "cx", "dx"); \
	} else { \
		*(otime) = *(ntime); \
	} \
	} while(0)

#else
#define CPU_CLOCKUPDATE(otime, ntime) \
		(*(otime) = *(ntime))
#endif

#endif /* _MACHINE_CLOCK_H_ */
