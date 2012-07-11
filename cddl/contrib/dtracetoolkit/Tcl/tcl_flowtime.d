#!/usr/sbin/dtrace -Zs
/*
 * tcl_flowtime.d - snoop Tcl execution showing procedure flow and delta times.
 *                  Written for the Tcl DTrace provider.
 *
 * $Id: tcl_flowtime.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * USAGE: tcl_flowtime.d		# hit Ctrl-C to end
 *
 * This watches Tcl method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		TIME(us)	Time since boot, us
 *		DELTA(us)	Elapsed time from previous line to this line
 *		CALL		Tcl command or procedure name
 *
 * LEGEND:
 *		->		procedure entry
 *		<-		procedure return
 *		 >		command entry
 *		 <		command return
 *
 * WARNING: Watch the first column carefully, it prints the CPU-id. If it
 * changes, then it is very likely that the output has been shuffled.
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
#pragma D option switchrate=10

self int depth;

dtrace:::BEGIN
{
	printf("%3s %6s %-16s %9s -- %s\n", "C", "PID", "TIME(us)",
	    "DELTA(us)", "CALL");
}

tcl*:::proc-entry,
tcl*:::proc-return,
tcl*:::cmd-entry,
tcl*:::cmd-return
/self->last == 0/
{
	self->last = timestamp;
}

tcl*:::proc-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %-16d %9d %*s-> %s\n", cpu, pid, timestamp / 1000,
	    this->delta, self->depth * 2, "", copyinstr(arg0));
	self->depth++;
	self->last = timestamp;
}

tcl*:::proc-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %-16d %9d %*s<- %s\n", cpu, pid, timestamp / 1000,
	    this->delta, self->depth * 2, "", copyinstr(arg0));
	self->last = timestamp;
}

tcl*:::cmd-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %-16d %9d %*s > %s\n", cpu, pid, timestamp / 1000,
	    this->delta, self->depth * 2, "", copyinstr(arg0));
	self->depth++;
	self->last = timestamp;
}

tcl*:::cmd-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %-16d %9d %*s < %s\n", cpu, pid, timestamp / 1000,
	    this->delta, self->depth * 2, "", copyinstr(arg0));
	self->last = timestamp;
}
