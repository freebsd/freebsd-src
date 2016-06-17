#ifndef _CRIS_DELAY_H
#define _CRIS_DELAY_H

/*
 * Copyright (C) 1998, 1999, 2000, 2001 Axis Communications AB
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */

#include <linux/config.h>
#include <linux/linkage.h>

#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif 

extern void __do_delay(void);	/* Special register call calling convention */

extern __inline__ void __delay(int loops)
{
	__asm__ __volatile__ (
			      "move.d %0,$r9\n\t"
			      "beq 2f\n\t"
			      "subq 1,$r9\n\t"
			      "1:\n\t"
			      "bne 1b\n\t"
			      "subq 1,$r9\n"
			      "2:"
			      : : "g" (loops) : "r9");
}


/* Use only for very small delays ( < 1 msec).  */

extern unsigned long loops_per_usec; /* arch/cris/mm/init.c */

extern __inline__ void udelay(unsigned long usecs)
{
	__delay(usecs * loops_per_usec);
}

/* ETRAX 100 is really too slow to sleep for nanosecs. */
/* Divide with 1024 to avoid a real division that would take >1 us... */
extern __inline__ void ndelay(unsigned long nsecs)
{
	__delay(1 + nsecs * loops_per_usec / 1024);
}

#endif /* defined(_CRIS_DELAY_H) */



