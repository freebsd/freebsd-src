#!/usr/sbin/dtrace -Zs
/*
 * rb_lines.d - trace Ruby line execution by process using DTrace.
 *              Written for the Ruby DTrace provider.
 *
 * $Id: rb_lines.d 20 2007-09-12 09:28:22Z brendan $
 *
 * This traces Ruby activity from all Ruby programs on the system that are
 * running with Ruby provider support.
 *
 * USAGE: rb_who.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the Ruby program
 *		LINE		Line number
 *		COUNT		Number of times a line was executed
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
	@calls[basename(copyinstr(arg0)), arg1] = count();
}

dtrace:::END
{
	printf(" %32s:%-6s %10s\n", "FILE", "LINE", "COUNT");
	printa(" %32s:%-6d %@10d\n", @calls);
}
