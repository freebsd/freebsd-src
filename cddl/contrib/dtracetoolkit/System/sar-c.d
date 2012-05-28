#!/usr/sbin/dtrace -s
/*
 * sar-c.d - sar -c demo in DTrace.
 *           Written using DTrace (Solaris 10 3/05).
 *
 * This has been written to demonstrate fetching similar data as sar -c
 * from DTrace. This program is intended as a starting point for other
 * DTrace scripts, by beginning with familiar statistics.
 *
 * $Id: sar-c.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	sar-c.d
 *
 * FIELDS:
 *		scall/s		System calls
 *		sread/s		reads
 *		swrit/s		writes
 *		fork/s		forks
 *		exec/s		execs
 *		rchar/s		read characters
 *		wchar/s		write characters
 *
 * IDEA: David Rubio, who also wrote the original.
 *
 * NOTES:
 *  As this program does not use Kstat, there is no summary since boot line.
 *
 * SEE ALSO:	sar(1)
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
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
 * 12-Jun-2005  Brendan Gregg   Created this.
 * 12-Jun-2005	   "      "	Last update.
 */

#pragma D option quiet

inline int SCREEN = 21;

/*
 * Initialise variables
 */
dtrace:::BEGIN
{
	scall = 0; sread = 0; swrit = 0; fork = 0; exec = 0;
	rchar = 0; wchar = 0;
	lines = SCREEN + 1;
}

/*
 * Print header
 */
dtrace:::BEGIN,
tick-1sec
/lines++ > SCREEN/
{
	printf("%-20s %7s %7s %7s %7s %7s %8s %8s\n",
	    "Time", "scall/s", "sread/s", "swrit/s", "fork/s",
	    "exec/s", "rchar/s", "wchar/s");
	lines = 0;
}

/*
 * Probe events
 */
syscall:::entry    { scall++; }
sysinfo:::sysread  { sread++; }
sysinfo:::syswrite { swrit++; }
sysinfo:::sysfork  { fork++;  }
sysinfo:::sysvfork { fork++;  }
sysinfo:::sysexec  { exec++;  }
sysinfo:::readch   { rchar += arg0; }
sysinfo:::writech  { wchar += arg0; }

/*
 * Print output line
 */
profile:::tick-1sec
{
	/* print line */
	printf("%20Y %7d %7d %7d %4d.00 %4d.00 %8d %8d\n",
	    walltimestamp, scall, sread, swrit, fork, exec, rchar, wchar);

	/* clear counters */
	scall = 0; sread = 0; swrit = 0; fork = 0; exec = 0;
	rchar = 0; wchar = 0;
}
