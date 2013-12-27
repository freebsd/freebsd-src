#!/usr/sbin/dtrace -Zs
/*
 * j_classflow.d - trace a Java class method flow using DTrace.
 *                 Written for the Java hotspot DTrace provider.
 *
 * $Id: j_classflow.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0) and the flag "+ExtendedDTraceProbes". eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_classflow.d	classname	# hit Ctrl-C to end
 *
 * This watches Java method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		TIME(us)	Time since boot, us
 *		PID		Process ID
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
#pragma D option defaultargs
#pragma D option switchrate=10

self int depth[int];

dtrace:::BEGIN
/$$1 == ""/
{
	printf("USAGE: j_classflow.d classname\n");
	exit(1);
}

dtrace:::BEGIN
{
	printf("%3s %6s %-16s -- %s\n", "C", "PID", "TIME(us)", "CLASS.METHOD");
}

hotspot*:::method-entry,
hotspot*:::method-return
{
	this->class = stringof((char *)copyin(arg1, arg2 + 1));
	this->class[arg2] = '\0';
}

hotspot*:::method-entry
/this->class == $$1/
{
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	printf("%3d %6d %-16d %*s-> %s.%s\n", cpu, pid, timestamp / 1000,
	    self->depth[arg0] * 2, "", stringof(this->class),
	    stringof(this->method));
	self->depth[arg0]++;
}

hotspot*:::method-return
/this->class == $$1/
{
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	self->depth[arg0] -= self->depth[arg0] > 0 ? 1 : 0;
	printf("%3d %6d %-16d %*s<- %s.%s\n", cpu, pid, timestamp / 1000,
	    self->depth[arg0] * 2, "", stringof(this->class),
	    stringof(this->method));
}
