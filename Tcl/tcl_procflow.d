#!/usr/sbin/dtrace -Zs
/*
 * tcl_procflow.d - snoop Tcl execution showing procedure flow using DTrace.
 *                  Written for the Tcl DTrace provider.
 *
 * $Id: tcl_procflow.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * USAGE: tcl_procflow.d	# hit Ctrl-C to end
 *
 * This watches Tcl method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		PID		Process ID
 *		PROCEDURE	Tcl procedure name
 *
 * LEGEND:
 *		->		proc entry
 *		<-		proc return
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
	printf("%3s %6s %-16s -- %s\n", "C", "PID", "TIME(us)", "PROCEDURE");
}

tcl*:::proc-entry
{
	printf("%3d %6d %-16d %*s-> %s\n", cpu, pid, timestamp / 1000,
	    self->depth * 2, "", copyinstr(arg0));
	self->depth++;
}

tcl*:::proc-return
{
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %-16d %*s<- %s\n", cpu, pid, timestamp / 1000,
	    self->depth * 2, "", copyinstr(arg0));
}
