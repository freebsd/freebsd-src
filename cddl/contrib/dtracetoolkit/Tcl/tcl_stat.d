#!/usr/sbin/dtrace -Zs
/*
 * tcl_stat.d - Tcl operation stats using DTrace.
 *            Written for the Tcl DTrace provider.
 *
 * $Id: tcl_stat.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * USAGE: tcl_stat.d [interval [count]]
 *
 * FIELDS:
 *		EXEC/s		Tcl programs executed per second, including
 *				those without Tcl provider support
 *		PROC/s		Procedures called, per second
 *		CMD/s		Commands created, per second
 *		OBJNEW/s	Objects created, per second
 *		OBJFRE/s	Objects freed, per second
 *		OP/s		Bytecode operations, per second
 *
 * The numbers are counts for the interval specified. The default interval
 * is 1 second.
 *
 * If you see a count in "EXECS" but not in the other columns, then you
 * may have older Tcl software that does not have the integrated DTrace
 * provider (or newer software where the provider has changed).
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
	execs = procs = cmds = objnew = objfree = ops = 0;
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
	printf("%-20s %6s %8s %8s %8s %8s %8s\n", "TIME", "EXEC/s",
	    "PROC/s", "CMD/s", "OBJNEW/s", "OBJFRE/s", "OP/s");
	lines = 0;
	first = 0;
}

/*
 * Tally Data
 */
proc:::exec-success
/execname == "tcl" || execname == "tclsh"/
{
	execs++;
}

tcl*:::proc-entry
{
	procs++;
}

tcl*:::cmd-entry
{
	cmds++;
}

tcl*:::obj-create
{
	objnew++;
}

tcl*:::obj-free
{
	objfree++;
}

tcl*:::inst-start
{
	ops++;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %6d %8d %8d %8d %8d %8d\n", walltimestamp,
	    execs / interval, procs / interval, cmds / interval,
	    objnew / interval, objfree / interval, ops / interval);
	execs = procs = cmds = objnew = objfree = ops = 0;
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
