#!/usr/sbin/dtrace -Zs
/*
 * js_stat.d - JavaScript operation stats using DTrace.
 *             Written for the JavaScript DTrace provider.
 *
 * $Id: js_stat.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all browsers on the system that are
 * running with JavaScript provider support.
 *
 * USAGE: js_stat.d [interval [count]]
 *
 * FIELDS:
 *		EXEC/s		JavaScript programs executed per second
 *		FUNCS/s		Functions called, per second
 *		OBJNEW/s	Objects created, per second
 *		OBJFRE/s	Objects freed (finalize), per second
 *
 * The numbers are counts for the interval specified. The default interval
 * is 1 second.
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
	execs = funcs = objnew = objfree = 0;
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
	printf("%-20s %8s %8s %8s %8s\n", "TIME", "EXEC/s", "FUNC/s",
	    "OBJNEW/s", "OBJFRE/s");
	lines = 0;
	first = 0;
}

/*
 * Tally Data
 */
javascript*:::execute-start
{
	execs++;
}

javascript*:::function-entry
{
	funcs++;
}

javascript*:::object-create-start
{
	objnew++;
}

javascript*:::object-finalize
{
	objfree++;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %8d %8d %8d %8d\n", walltimestamp, execs / interval,
	    funcs / interval, objnew / interval, objfree / interval);
	execs = funcs = objnew = objfree = 0;
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
