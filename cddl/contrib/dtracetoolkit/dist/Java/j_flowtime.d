#!/usr/sbin/dtrace -Zs
/*
 * j_flowtime.d - snoop Java execution with method flow and delta times.
 *                Written for the Java hotspot DTrace provider.
 *
 * $Id: j_flowtime.d 41 2007-09-17 02:20:10Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0) and the flag "+ExtendedDTraceProbes". eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_flowtime.d		# hit Ctrl-C to end
 *
 * This watches Java method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		TID		Thread ID
 *		TIME(us)	Time since boot (us)
 *		DELTA(us)	Elapsed time from previous line to this line
 *		CLASS.METHOD	Java class and method name
 *
 * LEGEND:
 *		->		method entry
 *		<-		method return
 *
 * WARNING: Watch the first column carefully, it prints the CPU-id. If it
 * changes, then it is very likely that the output has been shuffled.
 * Changes in TID will appear to shuffle output, as we change from one thread
 * depth to the next. See Docs/Notes/ALLjavaflow.txt for additional notes.
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

/* increasing bufsize can reduce drops */
#pragma D option bufsize=16m
#pragma D option quiet
#pragma D option switchrate=10

self int depth[int];

dtrace:::BEGIN
{
	printf("%3s %6s/%-5s %-16s %9s -- %s\n", "C", "PID", "TID", "TIME(us)",
	    "DELTA(us)", "CLASS.METHOD");
}

hotspot*:::method-entry,
hotspot*:::method-return
/self->last == 0/
{
	self->last = timestamp;
}

hotspot*:::method-entry
{
	this->delta = (timestamp - self->last) / 1000;
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	printf("%3d %6d/%-5d %-16d %9d %*s-> %s.%s\n", cpu, pid, tid,
	    timestamp / 1000, this->delta, self->depth[arg0] * 2, "",
	    stringof(this->class), stringof(this->method));
	self->depth[arg0]++;
	self->last = timestamp;
}

hotspot*:::method-return
{
	this->delta = (timestamp - self->last) / 1000;
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	self->depth[arg0] -= self->depth[arg0] > 0 ? 1 : 0;
	printf("%3d %6d/%-5d %-16d %9d %*s<- %s.%s\n", cpu, pid, tid,
	    timestamp / 1000, this->delta, self->depth[arg0] * 2, "",
	    stringof(this->class), stringof(this->method));
	self->last = timestamp;
}
