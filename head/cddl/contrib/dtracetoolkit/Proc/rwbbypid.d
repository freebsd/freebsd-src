#!/usr/sbin/dtrace -s
/*
 * rwbbypid.d - read/write bytes by PID.
 *              Written using DTrace (Solaris 10 3/05)
 *
 * This script tracks the bytes read and written at the syscall level
 * by processes, printing the totals in a report. This is tracking the
 * successful number of bytes read or written.
 *
 * $Id: rwbbypid.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	rwbbypid.d		# hit Ctrl-C to end sample
 *
 * FIELDS:
 *		PID		process ID
 *		CMD		process name
 *		DIR		direction, Read or Write
 *		BYTES		total bytes
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

sysinfo:::readch
{
	@bytes[pid, execname, "R"] = sum(arg0);
}

sysinfo:::writech
{
	@bytes[pid, execname, "W"] = sum(arg0);
}

dtrace:::END
{
	printf("%6s %-24s %4s %16s\n", "PID", "CMD", "DIR", "BYTES");
	printa("%6d %-24s %4s %@16d\n", @bytes);
}
