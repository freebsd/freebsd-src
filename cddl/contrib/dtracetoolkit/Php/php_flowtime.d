#!/usr/sbin/dtrace -Zs
/*
 * php_flowtime.d - snoop PHP functions with flow and delta times.
 *                  Written for the PHP DTrace provider.
 *
 * $Id: php_flowtime.d 53 2007-09-24 04:58:38Z brendan $
 *
 * This traces shell activity from PHP programs on the system that are
 * running with PHP provider support.
 *
 * USAGE: php_flowtime.d			# hit Ctrl-C to end
 *
 * This watches PHP function entries and returns, and indents child
 * function calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		FILE		Filename that this function belongs to
 *		DELTA(us)	Elapsed time from previous line to this line
 *		FUNC		PHP function name
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

self int last;

dtrace:::BEGIN
{
	printf("%3s %-16s %-16s %9s  -- %s\n", "C", "TIME(us)", "FILE",
	    "DELTA(us)", "FUNC");
}

php*:::function-entry,
php*:::function-return
/self->last == 0/
{
	self->last = timestamp;
}

php*:::function-entry
/arg0/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%3d %-16d %-16s %9d %*s-> %s\n", cpu, timestamp / 1000, 
	    basename(copyinstr(arg1)), this->delta, self->depth * 2, "",
	    copyinstr(arg0));
	self->depth++;
	self->last = timestamp;
}

php*:::function-return
/arg0/
{
	this->delta = (timestamp - self->last) / 1000;
	self->depth -= self->depth > 0 ? 1 : 0;
	printf("%3d %-16d %-16s %9d %*s<- %s\n", cpu, timestamp / 1000,
	    basename(copyinstr(arg1)), this->delta, self->depth * 2, "",
	    copyinstr(arg0));
	self->last = timestamp;
}
