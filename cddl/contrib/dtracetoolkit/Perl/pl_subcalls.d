#!/usr/sbin/dtrace -Zs
/*
 * pl_subcalls.d - measure Perl subroutine calls using DTrace.
 *                 Written for the Perl DTrace provider.
 *
 * $Id: pl_subcalls.d 25 2007-09-12 09:51:58Z brendan $
 *
 * This traces Perl activity from all running programs on the system
 * which support the Perl DTrace provider.
 *
 * USAGE: pl_subcalls.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *              FILE		Filename that contained the subroutine
 *		SUB		Perl subroutine name
 *		CALLS		Subroutine calls during this sample
 *
 * Filename and subroutine names are printed if available.
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
	@subs[basename(copyinstr(arg1)), copyinstr(arg0)] = count();
}

dtrace:::END
{
	printf(" %-32s %-32s %8s\n", "FILE", "SUB", "CALLS");
	printa(" %-32s %-32s %@8d\n", @subs);
}
