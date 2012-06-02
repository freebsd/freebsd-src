#!/usr/sbin/dtrace -Zs
/*
 * rb_flowtime.d - snoop Ruby function (method) flow using DTrace.
 *                 Written for the Ruby DTrace provider.
 *
 * $Id: rb_flowtime.d 41 2007-09-17 02:20:10Z brendan $
 *
 * This traces activity from all Ruby programs on the system that are
 * running with Ruby provider support.
 *
 * USAGE: rb_flowtime.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		FILE		Filename that this method belongs to
 *		DELTA(us)	Elapsed time from previous line to this line
 *		CLASS::METHOD	Ruby class and method name
 *
 * LEGEND:
 *		->		method entry
 *		<-		method return
 *
 * Filename and method names are printed if available.
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
	printf("%3s %-16s %-16s %9s -- %s\n", "C", "TIME(us)", "FILE",
	    "DELTA(us)", "CLASS::METHOD");
}

ruby*:::function-entry,
ruby*:::function-return
/self->last == 0/
{
	self->last = timestamp;
}

ruby*:::function-entry
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %-16d %-16s %9d %*s-> %s::%s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg2)), this->delta, self->depth * 2, "",
	    copyinstr(arg0), copyinstr(arg1));
	self->depth++;
	self->last = timestamp;
}

ruby*:::function-return
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %-16d %-16s %9d %*s<- %s::%s\n", cpu, timestamp / 1000,
	    basename(copyinstr(arg2)), this->delta, self->depth * 2, "",
	    copyinstr(arg0), copyinstr(arg1));
	self->last = timestamp;
}
