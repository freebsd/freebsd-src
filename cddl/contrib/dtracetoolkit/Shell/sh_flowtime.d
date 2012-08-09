#!/usr/sbin/dtrace -Zs
/*
 * sh_flowtime.d - snoop Bourne shell execution with flow and delta times.
 *                 Written for the sh DTrace provider.
 *
 * $Id: sh_flowtime.d 45 2007-09-17 08:54:56Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_flowtime.d			# hit Ctrl-C to end
 *
 * This watches shell function entries and returns, and indents child
 * function calls. Shell builtins are also printed.
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		FILE		Filename that this function belongs to
 *		NAME		Shell function or builtin name
 *
 * LEGEND:
 *		->		function entry
 *		<-		function return
 *		 >		builtin
 *		 |		external command
 *
 * DELTAs:
 *		->		previous line to the start of this function
 *		<-		previous line to the end of this function
 *		>		previous line to the end of this builtin
 *		|		previous line to the end of this command
 *
 * See sh_flowinfo.d for more verbose and more straightforward delta times.
 *
 * Filename and function names are printed if available.
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

self uint64_t last;

dtrace:::BEGIN
{
	printf("%3s %-16s %-16s %9s -- %s\n", "C", "TIME(us)", "FILE",
	    "DELTA(us)", "NAME");
}

sh*:::function-entry,
sh*:::function-return,
sh*:::builtin-return,
sh*:::command-return
/self->last == 0/
{
	self->last = timestamp;
}

sh*:::function-entry
{
	this->elapsed = (timestamp - self->last) / 1000;
	printf("%3d %-16d %-16s %9d %*s-> %s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg0)), this->elapsed, self->depth * 2, "",
	    copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

sh*:::function-return
{
	this->elapsed = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %-16d %-16s %9d %*s<- %s\n", cpu, timestamp / 1000,
	    basename(copyinstr(arg0)), this->elapsed, self->depth * 2, "",
	    copyinstr(arg1));
	self->last = timestamp;
}

sh*:::builtin-return
{
	this->elapsed = (timestamp - self->last) / 1000;
	printf("%3d %-16d %-16s %9d %*s> %s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg0)), this->elapsed, self->depth * 2, "",
	    copyinstr(arg1));
	self->last = timestamp;
}

sh*:::command-return
{
	this->elapsed = (timestamp - self->last) / 1000;
	printf("%3d %-16d %-16s %9d %*s| %s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg0)), this->elapsed, self->depth * 2, "",
	    copyinstr(arg1));
	self->last = timestamp;
}
