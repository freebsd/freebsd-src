#!/usr/sbin/dtrace -Zs
/*
 * sh_calls.d - count Bourne calls (func/builtin/cmd/subsh) using DTrace.
 *              Written for the sh DTrace provider.
 *
 * $Id: sh_calls.d 52 2007-09-24 04:28:01Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_calls.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the shell or shellscript
 *		TYPE		Type of call (func/builtin/cmd/subsh)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and function names are printed if available.
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

sh*:::function-entry
{
	@calls[basename(copyinstr(arg0)), "func", copyinstr(arg1)] = count();
}

sh*:::builtin-entry
{
	@calls[basename(copyinstr(arg0)), "builtin", copyinstr(arg1)] = count();
}

sh*:::command-entry
{
	@calls[basename(copyinstr(arg0)), "cmd", copyinstr(arg1)] = count();
}

sh*:::subshell-entry
/arg1 != 0/
{
	@calls[basename(copyinstr(arg0)), "subsh", "-"] = count();
}

dtrace:::END
{
	printf(" %-22s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "COUNT");
	printa(" %-22s %-10s %-32s %@8d\n", @calls);
}
