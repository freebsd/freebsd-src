#!/usr/sbin/dtrace -Zs
/*
 * sh_flowinfo.d - snoop Bourne shell flow with additional info.
 *                 Written for the sh DTrace provider.
 *
 * $Id: sh_flowinfo.d 52 2007-09-24 04:28:01Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_flowinfo.d			# hit Ctrl-C to end
 *
 * This watches shell function entries and returns, and indents child
 * function calls. Shell builtins and external commands are also printed.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		FILE		Filename of the shell script
 *		LINE		Line number of filename
 *		TYPE		Type of call (func/builtin/cmd/subsh)
 *		NAME		Shell function, builtin or command name
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
	self->depth = 0;
	printf("%3s %6s %10s  %16s:%-4s %-8s -- %s\n", "C", "PID", "DELTA(us)",
	    "FILE", "LINE", "TYPE", "NAME");
}

sh*:::function-entry,
sh*:::function-return,
sh*:::builtin-entry,
sh*:::builtin-return,
sh*:::command-entry,
sh*:::command-return,
sh*:::subshell-entry,
sh*:::subshell-return
/self->last == 0/
{
	self->last = timestamp;
}

sh*:::function-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %10d  %16s:%-4d %-8s %*s-> %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), arg2, "func",
	    self->depth * 2, "", copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

sh*:::function-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %10d  %16s:-    %-8s %*s<- %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), "func", self->depth * 2,
	    "", copyinstr(arg1));
	self->last = timestamp;
}

sh*:::builtin-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %10d  %16s:%-4d %-8s %*s-> %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), arg2, "builtin",
	    self->depth * 2, "", copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

sh*:::builtin-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %10d  %16s:-    %-8s %*s<- %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), "builtin",
	    self->depth * 2, "", copyinstr(arg1));
	self->last = timestamp;
}

sh*:::command-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %10d  %16s:%-4d %-8s %*s-> %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), arg2, "cmd",
	    self->depth * 2, "", copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

sh*:::command-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %10d  %16s:-    %-8s %*s<- %s\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), "cmd",
	    self->depth * 2, "", copyinstr(arg1));
	self->last = timestamp;
}

sh*:::subshell-entry
/arg1 != 0/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %6d %10d  %16s:-    %-8s %*s-> pid %d\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), "subsh",
	    self->depth * 2, "", arg1);
	self->depth++;
	self->last = timestamp;
}

sh*:::subshell-return
/self->last/
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %6d %10d  %16s:-    %-8s %*s<- = %d\n", cpu, pid,
	    this->delta, basename(copyinstr(arg0)), "subsh",
	    self->depth * 2, "", arg1);
	self->last = timestamp;
}
