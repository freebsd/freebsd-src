#!/usr/sbin/dtrace -Zs
/*
 * j_who.d - trace Java calls by process using DTrace.
 *           Written for the Java hotspot DTrace provider.
 *
 * $Id: j_who.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0).
 *
 * USAGE: j_who.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID of Java
 *		UID		User ID of the owner
 *		CALLS		Number of calls made (a measure of activity)
 *		ARGS		Process name and arguments
 *
 * The argument list is truncated at 55 characters (up to 80 is easily
 * available). To easily read the full argument list, use other system tools;
 * on Solaris use "pargs PID".
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

hotspot*:::Call*-entry
{
	@calls[pid, uid, curpsinfo->pr_psargs] = count();
}

dtrace:::END
{
	printf("   %6s %6s %6s %-55s\n", "PID", "UID", "CALLS", "ARGS");
	printa("   %6d %6d %@6d %-55.55s\n", @calls);
}
