#!/usr/sbin/dtrace -Zs
/*
 * pl_who.d - trace Perl subroutine execution by process using DTrace.
 *            Written for the Perl DTrace provider.
 *
 * $Id: pl_who.d 25 2007-09-12 09:51:58Z brendan $
 *
 * This traces Perl activity from all Perl programs on the system that are
 * running with Perl provider support.
 *
 * USAGE: pl_who.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID of Perl
 *		UID		User ID of the owner
 *		SUBS		Number of subroutine calls
 *		FILE		Pathname of the Perl program
 *
 * Filenames are printed if available.
 *
 * COPYRIGHT: Copyright (c) 2007 Brendan Gregg.
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
 * 09-Sep-2007	Brendan Gregg	Created this.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

perl*:::sub-entry
{
	@lines[pid, uid, copyinstr(arg1)] = count();
}

dtrace:::END
{
	printf("   %6s %6s %6s %s\n", "PID", "UID", "SUBS", "FILE");
	printa("   %6d %6d %@6d %s\n", @lines);
}
