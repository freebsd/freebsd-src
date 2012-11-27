#!/usr/sbin/dtrace -s
/*
 * pridist.d - process priority distribution.
 *             Written using DTrace (Solaris 10 3/05)
 *
 * This is a simple DTrace script that samples at 1000 Hz which process
 * is on the CPUs, and what the priority is. A distribution plot is printed.
 *
 * With priorities, the higher the priority the better chance the process
 * (actually, thread) has of being scheduled.
 *
 * This idea came from the script /usr/demo/dtrace/profpri.d, which
 * produces similar output for one particular PID.
 *
 * $Id: pridist.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       pridist.d       # hit Ctrl-C to end sampling
 *
 * FIELDS:
 *              CMD             process name
 *              PID             process ID
 *              value           process priority
 *              count           number of samples of at least this priority
 *
 * BASED ON: /usr/demo/dtrace/profpri.d
 *
 * SEE ALSO:
 *           DTrace Guide "profile Provider" chapter (docs.sun.com)
 *           dispadmin(1M)
 *
 * PORTIONS: Copyright (c) 2005 Brendan Gregg.
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
 * 13-Jun-2005	Brendan Gregg	Created this.
 * 22-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Sampling... Hit Ctrl-C to end.\n");
}

profile:::profile-1000hz
{
	@Count[execname, pid] = lquantize(curlwpsinfo->pr_pri, 0, 170, 5);
}

dtrace:::END
{
	printa(" CMD: %-16s PID: %d\n%@d\n", @Count);
}
