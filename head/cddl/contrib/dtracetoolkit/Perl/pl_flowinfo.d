#!/usr/sbin/dtrace -Zs
/*
 * pl_flowinfo.d - snoop Perl subroutine flow with info using DTrace.
 *                 Written for the Perl DTrace provider.
 *
 * $Id: pl_flowinfo.d 41 2007-09-17 02:20:10Z brendan $
 *
 * This traces activity from all Perl programs on the system that are
 * running with Perl provider support.
 *
 * USAGE: pl_flowinfo.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		FILE		Filename of the Perl program
 *		LINE		Line number of filename
 *		TYPE		Type of call (sub)
 *		SUB		Perl subroutine
 *
 * LEGEND:
 *		->		subroutine entry
 *		<-		subroutine return
 *
 * Filename and subroutine names are printed if available.
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
	printf("%s %6s %10s  %16s:%-4s %-8s -- %s\n", "C", "PID", "DELTA(us)",
	    "FILE", "LINE", "TYPE", "SUB");
}

perl*:::sub-entry,
perl*:::sub-return
/self->last == 0/
{
	self->last = timestamp;
}

perl*:::sub-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%d %6d %10d  %16s:%-4d %-8s %*s-> %s\n", cpu, pid, this->delta, 
	    basename(copyinstr(arg1)), arg2, "sub", self->depth * 2, "",
	    copyinstr(arg0));
	self->depth++;
	self->last = timestamp;
}

perl*:::sub-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%d %6d %10d  %16s:%-4d %-8s %*s<- %s\n", cpu, pid, this->delta, 
	    basename(copyinstr(arg1)), arg2, "sub", self->depth * 2, "",
	    copyinstr(arg0));
	self->last = timestamp;
}
