/*
 *	Precise Delay Loops for i386
 *
 *	Copyright (C) 1993 Linus Torvalds
 *	Copyright (C) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 *	The __delay function must _NOT_ be inlined as its execution time
 *	depends wildly on alignment on many x86 processors. The additional
 *	jump magic is needed to get the timing stable on all the CPU's
 *	we have to worry about.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/processor.h>
#include <asm/delay.h>

#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif

int x86_udelay_tsc = 0;		/* Delay via TSC */

	
/*
 *	Do a udelay using the TSC for any CPU that happens
 *	to have one that we trust.
 */

static void __rdtsc_delay(unsigned long loops)
{
	unsigned long bclock, now;
	
	rdtscl(bclock);
	do
	{
		rep_nop();
		rdtscl(now);
	} while ((now-bclock) < loops);
}

/*
 *	Non TSC based delay loop for 386, 486, MediaGX
 */
 
static void __loop_delay(unsigned long loops)
{
	int d0;
	__asm__ __volatile__(
		"\tjmp 1f\n"
		".align 16\n"
		"1:\tjmp 2f\n"
		".align 16\n"
		"2:\tdecl %0\n\tjns 2b"
		:"=&a" (d0)
		:"0" (loops));
}
extern void __cyclone_delay(unsigned long loops);
extern int use_cyclone;
void __delay(unsigned long loops)
{
	if (use_cyclone)
		__cyclone_delay(loops);
	else if (x86_udelay_tsc)
		__rdtsc_delay(loops);
	else
		__loop_delay(loops);
}

inline void __const_udelay(unsigned long xloops)
{
	int d0;
	__asm__("mull %0"
		:"=d" (xloops), "=&a" (d0)
		:"1" (xloops),"0" (current_cpu_data.loops_per_jiffy));
        __delay(xloops * HZ);
}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c6);  /* 2**32 / 1000000 */
}

void __ndelay(unsigned long nsecs)
{
	__const_udelay(nsecs * 0x00005);  /* 2**32 / 1000000000 (rounded up) */
}
