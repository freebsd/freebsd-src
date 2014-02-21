#!/usr/sbin/dtrace -s
/*
 * syscallbypid.d - report on syscalls by PID.
 *                  Written using DTrace (Solaris 10 3/05)
 *
 * $Id: syscallbypid.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	syscallbypid.d			# hit Ctrl-C to end sample
 *
 * FIELDS:
 *		PID		process ID
 *		CMD		process name
 *		SYSCALL		syscall name
 *		COUNT		number of syscalls for this PID
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
 * 15-May-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

syscall:::entry
{
	@num[pid, execname, probefunc] = count();
}

dtrace:::END
{
	printf("%6s %-24s %-24s %8s\n", "PID", "CMD", "SYSCALL", "COUNT");
	printa("%6d %-24s %-24s %@8d\n", @num);
}
