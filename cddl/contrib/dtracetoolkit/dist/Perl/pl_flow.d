#!/usr/sbin/dtrace -Zs
/*
 * pl_flow.d - snoop Perl execution showing subroutine flow.
 *             Written for the Solaris Perl DTrace provider.
 *
 * $Id: pl_flow.d 41 2007-09-17 02:20:10Z brendan $
 *
 * This traces Perl activity from all Perl programs on the system
 * running with Perl provider support.
 *
 * USAGE: pl_flow.d			# hit Ctrl-C to end
 *
 * This watches Perl subroutine entries and returns, and indents child
 * subroutine calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		FILE		Filename that this subroutine belongs to
 *		SUB		Subroutine name
 *
 * LEGEND:
 *		->		subroutine entry
 *		<-		subroutine return
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
	printf("%3s %-16s %-16s -- %s\n", "C", "TIME(us)", "FILE", "SUB");
}

perl*:::sub-entry
{
	printf("%3d %-16d %-16s %*s-> %s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg1)), self->depth * 2, "", copyinstr(arg0));
	self->depth++;
}

perl*:::sub-return
{
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %-16d %-16s %*s<- %s\n", cpu, timestamp / 1000,
	    basename(copyinstr(arg1)), self->depth * 2, "", copyinstr(arg0));
}
