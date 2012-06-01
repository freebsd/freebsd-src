#!/usr/sbin/dtrace -qs
/*
 * shortlived.d - determine time spent by short lived processes.
 *                Written in DTrace (Solaris 10 3/05).
 *
 * $Id: shortlived.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:    shortlived.d	# wait, then hit Ctrl-C
 *
 * Applications that run many short lived processes can cause load
 * on the system that is difficult to identify - the processes
 * aren't sampled in time by programs such as prstat. This program
 * illustrates how much time was spent processing those extra
 * processes, and a table of process name by total times for each.
 *
 * SEE ALSO: execsnoop
 *
 * Notes:
 * - The measurements are minimum values, not all of the overheads
 *   caused by process generation and destruction are measured (DTrace
 *   can do so, but the script would become seriously complex).
 * - The summary values are accurate, the by program and by PPID values
 *   are usually slightly smaller due to rounding errors.
 *
 * COPYRIGHT: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 22-Apr-2005  Brendan Gregg   Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

/*
 * Start
 */
dtrace:::BEGIN
{
	/* save start time */
	start = timestamp;

	/* this is time spent on shortlived processes */
	procs = 0;

	/* print header */
	printf("Tracing... Hit Ctrl-C to stop.\n");
}

/*
 * Measure parent fork time
 */
syscall::*fork*:entry
{
	/* save start of fork */
	self->fork = vtimestamp;
}
syscall::*fork*:return
/arg0 != 0 && self->fork/
{
	/* record elapsed time for the fork syscall */
	this->elapsed = vtimestamp - self->fork;
	procs += this->elapsed;
	self->fork = 0;
}

/*
 * Measure child processes time
 */
syscall::*fork*:return
/arg0 == 0/
{
	/* save start of child process */
	self->start = vtimestamp;

	/* memory cleanup */
	self->fork = 0;
}
proc:::exit
/self->start/
{
	/* record elapsed time for process execution */
	this->elapsed = vtimestamp - self->start;
	procs += this->elapsed;

	/* sum elapsed by process name and ppid */
	@Times_exec[execname] = sum(this->elapsed/1000000);
	@Times_ppid[ppid] = sum(this->elapsed/1000000);

	/* memory cleanup */
	self->start = 0;
}

/*
 * Print report
 */
dtrace:::END
{
	this->total = timestamp - start;
	printf("short lived processes: %6d.%03d secs\n",
	    procs/1000000000, (procs%1000000000)/1000000);
	printf("total sample duration: %6d.%03d secs\n",
	    this->total/1000000000, (this->total%1000000000)/1000000);
	printf("\nTotal time by process name,\n");
	printa("%18s %@12d ms\n", @Times_exec);
	printf("\nTotal time by PPID,\n");
	printa("%18d %@12d ms\n", @Times_ppid);
}
