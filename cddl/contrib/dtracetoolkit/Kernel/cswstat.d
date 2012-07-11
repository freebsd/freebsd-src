#!/usr/sbin/dtrace -s
/*
 * cswstat.d - context switch time stat.
 *	       Uses DTrace (Solaris 10 03/05)
 *
 * This prints a context switch count and consumed time for context
 * switching every second.
 *
 * $Id: cswstat.d 15 2007-09-11 09:09:25Z brendan $
 *
 * USAGE:	cswstat.d
 *
 * FIELDS:
 *		TIME		Current time
 *		NUM		Number of context switches
 *		CSWTIME(us)	Time consumed context switching, us
 *		AVGTIME(us)	Average context switch time, us
 *
 * THANKS: Toomas Soome
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 17-May-2005  Brendan Gregg   Created this.
 * 03-Nov-2005	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	/* print header */
	printf("%-20s  %8s %12s %12s\n", "TIME", "NUM", "CSWTIME(us)",
	    "AVGTIME(us)");
	times = 0;
	num = 0;
}

sched:::off-cpu
{
	/* csw start */
	num++;
	start[cpu] = timestamp;
}

sched:::on-cpu
/start[cpu]/
{
	/* csw end */
	times += timestamp - start[cpu];
	start[cpu] = 0;
}

profile:::tick-1sec
{
	/* print output */
	printf("%20Y  %8d %12d %12d\n", walltimestamp, num, times/1000,
	    num == 0 ? 0 : times/(1000 * num));
	times = 0;
	num = 0;
}
