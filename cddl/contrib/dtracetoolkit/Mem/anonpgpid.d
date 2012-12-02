#!/usr/sbin/dtrace -Cs
/*
 * anonpgpid.d - anonymous memory paging info by process on CPU.
 *               Written using DTrace (Solaris 10 3/05).
 *
 * This scripts may help identify which processes are affected by a system
 * with low memory, which is paging to the physical swap device. A report
 * of the process on the CPU when paging occured is printed.
 *
 * $Id: anonpgpid.d 8 2007-08-06 05:55:26Z brendan $
 *
 * USAGE:	anonpgpid.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		CMD		Process name
 *		D		Direction, Read or Write
 *		BYTES		Total bytes during sample
 *
 * NOTES:
 *
 * This program is currently an approximation - often the process when writing
 * pages to swap will be "pageout" the pageout scanner, or "rcapd" the
 * resource capping daemon.
 *
 * THANKS: James Dickens
 *
 * COPYRIGHT: Copyright (c) 2006 Brendan Gregg.
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
 * TODO:
 *
 * Track processes accurately. This is a little difficult - anonpgout
 * occurs asynchronously to the process, and events related to this don't
 * point back to the process.
 *
 * Author: Brendan Gregg  [Sydney, Australia]
 *
 * 25-Jul-2005	Brendan Gregg	Created this.
 * 18-Feb-2006	   "      "	Last update.
 */

#include <sys/vnode.h>

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

fbt::pageio_setup:entry
/((args[2]->v_flag & (VISSWAP | VSWAPLIKE)) != 0)/
{
	@total[pid, execname, args[3] & B_READ ? "R" : "W"] = sum(arg1);
}

dtrace:::END
{
	printf("%6s %-16s %1s %s\n", "PID", "CMD", "D", "BYTES");
	printa("%6d %-16s %1s %@d\n", @total);
}
