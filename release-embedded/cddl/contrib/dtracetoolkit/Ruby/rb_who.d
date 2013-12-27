#!/usr/sbin/dtrace -Zs
/*
 * rb_who.d - trace Ruby line execution by process using DTrace.
 *            Written for the Ruby DTrace provider.
 *
 * $Id: rb_who.d 49 2007-09-17 12:03:20Z brendan $
 *
 * This traces Ruby activity from all Ruby programs on the system that are
 * running with Ruby provider support.
 *
 * USAGE: rb_who.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID of Ruby
 *		UID		User ID of the owner
 *		LINES		Number of times a line was executed
 *		FILE		Pathname of the Ruby program
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

ruby*:::line
{
	@lines[pid, uid, copyinstr(arg0)] = count();
}

dtrace:::END
{
	printf("   %6s %6s %10s %s\n", "PID", "UID", "LINES", "FILE");
	printa("   %6d %6d %@10d %s\n", @lines);
}
