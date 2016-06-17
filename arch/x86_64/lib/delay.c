/*
 *	Precise Delay Loops for i386
 *
 *	Copyright (C) 1993 Linus Torvalds
 *	Copyright (C) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 *	The __delay function must _NOT_ be inlined as its execution time
 *	depends wildly on alignment on many x86 processors. 
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/delay.h>

#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif

void __delay(unsigned long loops)
{
	unsigned long bclock, now;
	
	rdtscl(bclock);
	do
	{
		rep_nop(); 
		rdtscl(now);
	}
	while((now-bclock) < loops);
}

inline void __const_udelay(unsigned long xloops)
{
        __delay(((xloops * current_cpu_data.loops_per_jiffy) >> 32) * HZ);
}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c6);  /* 2**32 / 1000000 */
}

void __ndelay(unsigned long usecs)
{
       __const_udelay(usecs * 0x00005);  /* 2**32 / 1000000000 (rounded up) */
}

