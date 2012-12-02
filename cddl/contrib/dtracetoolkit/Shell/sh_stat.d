#!/usr/sbin/dtrace -Zs
/*
 * sh_stat.d - Bourne shell operation stats using DTrace.
 *             Written for the sh DTrace provider.
 *
 * $Id: sh_stat.d 52 2007-09-24 04:28:01Z brendan $
 *
 * This traces activity from all sh processes on the system that are running
 * with sh provider support.
 *
 * USAGE: sh_stat.d [interval [count]]
 *
 * FIELDS:
 *		EXEC/s		Bourne shells executed per second, including
 *				those without sh provider support
 *		FUNC/s		Functions called, per second
 *		BLTIN/s		Builtins called, per second
 *		SUB-SH/s	Sub-shells called, per second
 *		CMD/s		External commands called, per second
 *
 * The numbers are counts for the interval specified. The default interval
 * is 1 second.
 *
 * If you see a count in "EXECS" but not in the other columns, then sh
 * scripts may be running without the DTrace sh provider. See Shell/Readme.
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
#pragma D option defaultargs

inline int SCREEN = 21;

dtrace:::BEGIN
{
	execs = funcs = builtins = subs = cmds = 0;
	lines = SCREEN + 1;
	interval = $1 ? $1 : 1;
	counts = $2 ? $2 : -1;
	secs = interval;
	first = 1;
}

profile:::tick-1sec
{
	secs--;
}

/*
 * Print Header
 */
dtrace:::BEGIN,
profile:::tick-1sec
/first || (secs == 0 && lines > SCREEN)/
{
	printf("%-20s %8s %8s %8s %8s %8s\n", "TIME", "EXEC/s", "FUNCS/s",
	    "BLTINS/s", "SUB-SH/s", "CMD/s");
	lines = 0;
	first = 0;
}

/*
 * Tally Data
 */
proc:::exec-success
/execname == "sh"/
{
	execs++;
}

sh*:::function-entry
{
	funcs++;
}

sh*:::builtin-entry
{
	builtins++;
}

sh*:::subshell-entry
/arg0 != 0/
{
	subs++;
}

sh*:::command-entry
{
	cmds++;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %8d %8d %8d %8d %8d\n", walltimestamp, execs / interval,
	    funcs / interval, builtins / interval, subs / interval,
	    cmds / interval);
	execs = funcs = builtins = subs = cmds = 0;
	secs = interval;
	lines++;
	counts--;
}

/*
 * End
 */
profile:::tick-1sec
/counts == 0/
{
        exit(0);
}
