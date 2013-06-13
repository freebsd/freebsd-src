#!/usr/sbin/dtrace -Zs
/*
 * rb_stat.d - Ruby operation stats using DTrace.
 *             Written for the Ruby DTrace provider.
 *
 * $Id: rb_stat.d 20 2007-09-12 09:28:22Z brendan $
 *
 * This traces activity from all Ruby programs on the system that are
 * running with Ruby provider support.
 *
 * USAGE: rb_stat.d [interval [count]]
 *
 * FIELDS:
 *		EXEC/s		Ruby programs executed per second, including
 *				those without Ruby provider support
 *		METHOD/s	Methods called, per second
 *		OBJNEW/s	Objects created, per second
 *		OBJFRE/s	Objects freed, per second
 *		RAIS/s		Raises, per second
 *		RESC/s		Rescues, per second
 *		GC/s		Garbage collects, per second
 *
 * The numbers are counts for the interval specified. The default interval
 * is 1 second.
 *
 * If you see a count in "EXECS" but not in the other columns, then your
 * Ruby software is probably not running with the DTrace Ruby provider.
 * See Ruby/Readme.
 *
 * Filename and method names are printed if available.
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
	execs = methods = objnew = objfree = gc = raised = rescue = 0;
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
	printf("%-20s %8s %8s %8s %8s %6s %6s %6s\n", "TIME", "EXEC/s",
	    "METHOD/s", "OBJNEW/s", "OBJFRE/s", "RAIS/s", "RESC/s", "GC/s");
	lines = 0;
	first = 0;
}

/*
 * Tally Data
 */
proc:::exec-success
/execname == "ruby"/
{
	execs++;
}

ruby*:::function-entry
{
	methods++;
}

ruby*:::object-create-start
{
	objnew++;
}

ruby*:::object-free
{
	objfree++;
}

ruby*:::raise
{
	raised++;
}

ruby*:::rescue
{
	rescue++;
}

ruby*:::gc-begin
{
	gc++;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %8d %8d %8d %8d %6d %6d %6d\n", walltimestamp,
	    execs / interval, methods / interval, objnew / interval,
	    objfree / interval, raised / interval, rescue / interval,
	    gc / interval);
	execs = methods = objnew = objfree = gc = raised = rescue = 0;
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
