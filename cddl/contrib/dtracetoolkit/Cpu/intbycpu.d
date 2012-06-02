#!/usr/sbin/dtrace -s
/*
 * intbycpu.d - interrupts by CPU.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * $Id: intbycpu.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	intbycpu.d		# hit Ctrl-C to end sample
 *
 * FIELDS:
 *		CPU		CPU number
 *		INTERRUPTS	number of interrupts in sample
 *
 * This is based on a DTrace OneLiner from the DTraceToolkit.
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

sdt:::interrupt-start { @num[cpu] = count(); }

dtrace:::END
{
	printf("%-16s %16s\n", "CPU", "INTERRUPTS");
	printa("%-16d %@16d\n", @num);
}
