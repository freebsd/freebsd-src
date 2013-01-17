#!/usr/sbin/dtrace -Zs
/*
 * pl_syscolors.d - trace Perl subroutine flow plus syscalls, in color.
 *                  Written for the Perl DTrace provider.
 *
 * $Id: pl_syscolors.d 27 2007-09-13 09:26:01Z brendan $
 *
 * USAGE: pl_syscolors.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * This watches Perl subroutine entries and returns, and indents child
 * subroutine calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		FILE		Filename of the Perl program
 *		LINE		Line number of filename
 *		TYPE		Type of call (sub/syscall)
 *		NAME		Perl subroutine or syscall name
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
	/*
	 * The following are terminal color escape sequences.
	 * Change them to whatever you prefer, eg HTML font tags.
	 */
        color_perl = "\033[2;35m";		/* violet, faint */
        color_syscall = "\033[2;32m";		/* green, faint */
        color_off = "\033[0m";			/* default */

	printf("%s %6s %10s  %16s:%-4s %-8s -- %s\n", "C", "PID", "DELTA(us)",
	    "FILE", "LINE", "TYPE", "NAME");
}

perl$target:::sub-entry,
perl$target:::sub-return,
syscall:::entry,
syscall:::return
/self->last == 0 && pid == $target/
{
	self->last = timestamp;
}

perl$target:::sub-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%d %6d %10d  %16s:%-4d %-8s %*s-> %s%s\n", color_perl,
	    cpu, pid, this->delta, basename(copyinstr(arg0)), arg2, "sub",
	    self->depth * 2, "", copyinstr(arg1), color_off);
	self->depth++;
	self->last = timestamp;
}

perl$target:::sub-return
{
	this->delta = (timestamp - self->last) / 1000;
	this->name = strjoin(strjoin(copyinstr(arg0), "::"), copyinstr(arg1));
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%s%d %6d %10d  %16s:%-4d %-8s %*s<- %s%s\n", color_perl,
	    cpu, pid, this->delta, basename(copyinstr(arg0)), arg2, "sub",
	    self->depth * 2, "", copyinstr(arg1), color_off);
	self->last = timestamp;
}

syscall:::entry
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%d %6d %10d  %16s:-    %-8s %*s-> %s%s\n", color_syscall,
	    cpu, pid, this->delta, "\"", "syscall", self->depth * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

syscall:::return
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%d %6d %10d  %16s:-    %-8s %*s<- %s%s\n", color_syscall,
	    cpu, pid, this->delta, "\"", "syscall", self->depth * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

proc:::exit
/pid == $target/
{
	exit(0);
}
