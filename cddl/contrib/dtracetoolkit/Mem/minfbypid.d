#!/usr/sbin/dtrace -s
/*
 * minfbypid.d - minor faults by PID.
 *               Written using DTrace (Solaris 10 3/05)
 *
 * This program prints a report of minor faults by PID. Minor faults are
 * an indiction of memory consumption. This script could be used to help
 * determine which process was consuming the most memory during the sample.
 *
 * $Id: minfbypid.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	minfbypid.d		# hit Ctrl-C to end sample
 *
 * FIELDS:
 *		PID		process ID
 *		CMD		process name
 *		MINFAULTS	number of minor faults
 *
 * This is based on a script from DExplorer.
 *
 * COPYRIGHT: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 28-Jun-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

vminfo:::as_fault
{
	@mem[pid, execname] = sum(arg0);
}

dtrace:::END
{
	printf("%6s %-16s %16s\n", "PID", "CMD", "MINFAULTS");
	printa("%6d %-16s %@16d\n", @mem);
}
