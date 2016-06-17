/*
 * Copyright (C) 1993, 2000 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <asm/param.h>
#include <asm/smp.h>
#include <linux/delay.h>

/*
 * Use only for very small delays (< 1 msec). 
 *
 * The active part of our cycle counter is only 32-bits wide, and
 * we're treating the difference between two marks as signed.  On
 * a 1GHz box, that's about 2 seconds.
 */

void __delay(int loops)
{
	long long dummy;
	__asm__ __volatile__("gettr	" __t0 ", %1\n\t"
			     "_pta	4, " __t0 "\n\t"
			     "addi	%0, -1, %0\n\t"
			     "bne	%0, r63, " __t0 "\n\t"
			     "ptabs	%1, " __t0 "\n\t":"=r"(loops),
			     "=r"(dummy)
			     :"0"(loops));
}

void __udelay(unsigned long long usecs, unsigned long lpj)
{
	usecs *= (((unsigned long long) HZ << 32) / 1000000) * lpj;
	__delay((long long) usecs >> 32);
}

void __ndelay(unsigned long long nsecs, unsigned long lpj)
{
	nsecs *= (((unsigned long long) HZ << 32) / 1000000000) * lpj;
	__delay((long long) nsecs >> 32);
}

void udelay(unsigned long usecs)
{
	__udelay(usecs, loops_per_jiffy);
}

void ndelay(unsigned long nsecs)
{
	__ndelay(nsecs, loops_per_jiffy);
}

