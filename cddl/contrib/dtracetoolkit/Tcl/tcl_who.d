#!/usr/sbin/dtrace -Zs
/*
 * tcl_who.d - trace Tcl calls by process using DTrace.
 *           Written for the Tcl DTrace provider.
 *
 * $Id: tcl_who.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * USAGE: tcl_who.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID of Tcl
 *		UID		User ID of the owner
 *		CALLS		Number of calls made (proc + cmd)
 *		ARGS		Process name and arguments
 *
 * Calls is a measure of activity, and is a count of the procedures and
 * commands that Tcl called.
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

tcl*:::proc-entry,
tcl*:::cmd-entry
{
	@calls[pid, uid, curpsinfo->pr_psargs] = count();
}

dtrace:::END
{
	printf("   %6s %6s %6s %-55s\n", "PID", "UID", "CALLS", "ARGS");
	printa("   %6d %6d %@6d %-55.55s\n", @calls);
}
