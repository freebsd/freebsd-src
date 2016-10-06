/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	from OpenSolaris "tick.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tick.c	1.3 (gritter) 10/22/05
 */

/* time programs */
# include "sys/times.h"
# include "time.h"
# include "stdio.h"
# include "sys/types.h"

struct tbuffer {
	long	proc_user_time;
	long	proc_system_time;
	long	child_user_time;
	long	child_system_time;
};
static long start, user, systm;
void
tick(void)
{
	struct tms tx;
	time_t tp;
	times (&tx);
	time (&tp);
	user =  tx.tms_utime;
	systm= tx.tms_stime;
	start = tp;
}
void
tock(void)
{
	struct tms tx;
	time_t tp;
	float lap, use, sys;
	if (start==0) return;
	times (&tx);
	time (&tp);
	lap = (tp - start)/60.;
	use = (tx.tms_utime - user)/60.;
	sys = (tx.tms_stime - systm)/60.;
	printf("Elapsed %.2f CPU %.2f (user %.2f, sys %.2f)\n",
		lap, use+sys, use, sys);
}
