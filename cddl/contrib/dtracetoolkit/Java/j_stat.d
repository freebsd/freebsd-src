#!/usr/sbin/dtrace -Zs
/*
 * j_stat.d - Java operation stats using DTrace.
 *            Written for the Java hotspot DTrace provider.
 *
 * $Id: j_stat.d 64 2007-10-04 08:35:29Z claire $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0). Method calls and object allocation are only
 * visible when using the flag "+ExtendedDTraceProbes". eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_stat.d [interval [count]]
 *
 * FIELDS:
 *		EXEC/s		Java programs executed per second, including
 *				those without Java provider support
 *		THREAD/s	Threads created, per second
 *		METHOD/s	Methods called, per second
 *		OBJNEW/s	Objects created, per second
 *		CLOAD/s		Class loads, per second
 *		EXCP/s		Exceptions raised, per second
 *		GC/s		Garbage collects, per second
 *
 * The numbers are per second counts for the interval specified. The default 
 * interval is 1 second.
 *
 * If you see a count in "EXECS" but not in the other columns, then your
 * Java software is probably not running with the DTrace hotspot provider.
 *
 * If you see counts in "CLOAD" but not in "METHODS", then you Java
 * software probably isn't running with "+ExtendedDTraceProbes".
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
	execs = threads = methods = objnew = cload = gc = exception = 0;
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
	printf("%-20s %6s %8s %8s %8s %8s %6s %6s\n", "TIME", "EXEC/s",
	    "THREAD/s", "METHOD/s", "OBJNEW/s", "CLOAD/s", "EXCP/s", "GC/s");
	lines = 0;
	first = 0;
}

/*
 * Tally Data
 */
proc:::exec-success
/execname == "java"/
{
	execs++;
}

hotspot*:::thread-start
{
	threads++;
}

hotspot*:::method-entry
{
	methods++;
}

hotspot*:::object-alloc
{
	oalloc++;
}

hotspot*:::class-loaded
{
	cload++;
}

hotspot*:::gc-begin
{
	gc++;
}

hotspot*:::ExceptionOccurred-entry
{
	exception++;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %6d %8d %8d %8d %8d %6d %6d\n", walltimestamp,
	    execs / interval, threads / interval, methods / interval,
	    oalloc / interval, cload / interval, exception / interval,
	    gc / interval);
	execs = threads = methods = oalloc = cload = gc = exception = 0;
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
