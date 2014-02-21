#!/usr/sbin/dtrace -s
/*
 * priclass.d - priority distribution by scheduling class.
 *              Written using DTrace (Solaris 10 3/05)
 *
 * This is a simple DTrace script that samples at 1000 Hz the current
 * thread's scheduling class and priority. A distribution plot is printed.
 *
 * With priorities, the higher the priority the better chance the thread
 * has of being scheduled.
 *
 * This idea came from the script /usr/demo/dtrace/pri.d, which
 * produces similar output for priority changes, not samples.
 *
 * $Id: priclass.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       priclass.d      # hit Ctrl-C to end sampling
 *
 * FIELDS:
 *              value           process priority
 *              count           number of samples of at least this priority
 *
 * Also printed is the scheduling class,
 *
 *		TS		time sharing
 *		IA		interactive
 *		RT		real time
 *		SYS		system
 *		FSS		fair share schedular
 *
 * BASED ON: /usr/demo/dtrace/pri.d
 *
 * SEE ALSO: DTrace Guide "profile Provider" chapter (docs.sun.com)
 *           dispadmin(1M)
 *
 * PORTIONS: Copyright (c) 2006 Brendan Gregg.
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
 * 12-Feb-2006	Brendan Gregg	Created this.
 * 22-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Sampling... Hit Ctrl-C to end.\n");
}

profile:::profile-1000hz
{
	@count[stringof(curlwpsinfo->pr_clname)]
	    = lquantize(curlwpsinfo->pr_pri, 0, 170, 10);
}
