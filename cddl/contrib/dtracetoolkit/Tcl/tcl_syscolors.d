#!/usr/sbin/dtrace -Zs
/*
 * tcl_syscolors.d - trace Tcl program flow plus syscalls, in color.
 *                   Written for the Tcl DTrace provider.
 *
 * $Id: tcl_syscolors.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * USAGE: tcl_syscolors.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * This watches Tcl method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		TID		Thread ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		TYPE		Type of call (proc/cmd/syscall)
 *		NAME		Tcl proc/cmd or syscall name
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
        color_tcl = "\033[2;35m";		/* violet, faint */
        color_line = "\033[1;35m";		/* violet, bold */
        color_syscall = "\033[2;32m";		/* green, faint */
        color_off = "\033[0m";			/* default */

	printf("%3s %6s %9s %-8s -- %s\n", "C", "PID", "DELTA(us)", "TYPE",
	    "NAME");
}

tcl$target:::method-entry,
tcl$target:::method-return,
syscall:::entry,
syscall:::return
/self->last == 0 && pid == $target/
{
	self->last = timestamp;
}

tcl$target:::proc-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d %9d %-8s %*s-> %s%s\n", color_tcl, cpu,
	    pid, this->delta, "proc", self->depth * 2, "", copyinstr(arg0),
	    color_off);
	self->depth++;
	self->depthlast = self->depth;
	self->last = timestamp;
}

tcl$target:::proc-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%s%3d %6d %9d %-8s %*s<- %s%s\n", color_tcl, cpu,
	    pid, this->delta, "proc", self->depth * 2, "", copyinstr(arg0),
	    color_off);
	self->depthlast = self->depth;
	self->last = timestamp;
}

tcl$target:::cmd-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d %9d %-8s %*s-> %s%s\n", color_tcl, cpu,
	    pid, this->delta, "cmd", self->depth * 2, "", copyinstr(arg0),
	    color_off);
	self->depth++;
	self->depthlast = self->depth;
	self->last = timestamp;
}

tcl$target:::cmd-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%s%3d %6d %9d %-8s %*s<- %s%s\n", color_tcl, cpu,
	    pid, this->delta, "cmd", self->depth * 2, "", copyinstr(arg0),
	    color_off);
	self->depthlast = self->depth;
	self->last = timestamp;
}

syscall:::entry
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d %9d %-8s %*s-> %s%s\n", color_syscall,
	    cpu, pid, this->delta, "syscall", self->depthlast * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

syscall:::return
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d %9d %-8s %*s<- %s%s\n", color_syscall,
	    cpu, pid, this->delta, "syscall", self->depthlast * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

proc:::exit
/pid == $target/
{
	exit(0);
}
