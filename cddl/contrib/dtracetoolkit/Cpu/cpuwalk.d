#!/usr/sbin/dtrace -s
/*
 * cpuwalk.d - Measure which CPUs a process runs on.
 *             Written using DTrace (Solaris 10 3/05)
 *
 * This program is for multi-CPU servers, and can help identify if a process
 * is running on multiple CPUs concurrently or not.
 *
 * $Id: cpuwalk.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	cpuwalk.d [duration]
 *	   eg,
 *		cpuwalk.d 10		# sample for 10 seconds
 *		cpuwalk.d		# sample until Ctrl-C is hit
 *
 * FIELDS:
 *		value		CPU id
 *		count		Number of 1000 hz samples on this CPU
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
 * 22-Sep-2005  Brendan Gregg   Created this.
 * 14-Feb-2006	   "      "	Last update.
 */

#pragma D option quiet
#pragma D option defaultargs

inline int MAXCPUID = 1024;

dtrace:::BEGIN
{
	$1 ? printf("Sampling...\n") :
	    printf("Sampling... Hit Ctrl-C to end.\n");
	seconds = 0;
}

profile:::profile-1000hz
/pid/
{
	@sample[pid, execname] = lquantize(cpu, 0, MAXCPUID, 1);
}

profile:::tick-1sec
{
	seconds++;
}

profile:::tick-1sec
/seconds == $1/
{
	exit(0);
}

dtrace:::END
{
	printa("\n     PID: %-8d CMD: %s\n%@d", @sample);
}
