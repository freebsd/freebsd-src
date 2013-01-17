#!/usr/sbin/dtrace -Zs
/*
 * py_flowinfo.d - snoop Python function flow with info using DTrace.
 *                 Written for the Python DTrace provider.
 *
 * $Id: py_flowinfo.d 41 2007-09-17 02:20:10Z brendan $
 *
 * This traces activity from all Python programs on the system that are
 * running with Python provider support.
 *
 * USAGE: py_flowinfo.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		FILE		Filename of the Python program
 *		LINE		Line number of filename
 *		TYPE		Type of call (func)
 *		FUNC		Python function
 *
 * LEGEND:
 *		->		function entry
 *		<-		function return
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

dtrace:::BEGIN
{
	printf("%s %6s %10s  %16s:%-4s %-8s -- %s\n", "C", "PID", "DELTA(us)",
	    "FILE", "LINE", "TYPE", "FUNC");
}

python*:::function-entry,
python*:::function-return
/self->last == 0/
{
	self->last = timestamp;
}

python*:::function-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%d %6d %10d  %16s:%-4d %-8s %*s-> %s\n", cpu, pid, this->delta, 
	    basename(copyinstr(arg0)), arg2, "func", self->depth * 2, "",
	    copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

python*:::function-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%d %6d %10d  %16s:%-4d %-8s %*s<- %s\n", cpu, pid, this->delta, 
	    basename(copyinstr(arg0)), arg2, "func", self->depth * 2, "",
	    copyinstr(arg1));
	self->last = timestamp;
}
