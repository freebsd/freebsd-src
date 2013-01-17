#!/usr/sbin/dtrace -Zs
/*
 * j_syscolors.d - trace Java method flow plus syscalls, in color.
 *                 Written for the Java hotspot DTrace provider.
 *
 * $Id: j_syscolors.d 58 2007-10-01 13:36:29Z brendan $
 *
 * This traces Java methods if the hotspot provider exists (1.6.0) and
 * the flag "+ExtendedDTraceProbes" is used. eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_syscolors.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * This watches Java method entries and returns, and indents child
 * method calls.
 *
 * FIELDS:
 *		C		CPU-id
 *		PID		Process ID
 *		TID		Thread ID
 *		DELTA(us)	Elapsed time from previous line to this line
 *		TYPE		Type of call (func/syscall)
 *		NAME		Java method or syscall name
 *
 * If the flow appears to jump, check the TID column - the JVM may have
 * switched to another thread.
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
#pragma D option bufsize=32m
#pragma D option quiet
#pragma D option switchrate=10

self int depth[int];

dtrace:::BEGIN
{
        color_java = "\033[2;35m";		/* violet, faint */
        color_line = "\033[1;35m";		/* violet, bold */
        color_syscall = "\033[2;32m";		/* green, faint */
        color_off = "\033[0m";			/* default */

	printf("%3s %6s/%-5s %9s %-8s -- %s\n", "C", "PID", "TID", "DELTA(us)",
	    "TYPE", "NAME");
}

hotspot$target:::method-entry,
hotspot$target:::method-return,
syscall:::entry,
syscall:::return
/self->last == 0 && pid == $target/
{
	self->last = timestamp;
}

hotspot$target:::method-entry
{
	this->delta = (timestamp - self->last) / 1000;
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	printf("%s%3d %6d/%-5d %9d %-8s %*s-> %s.%s%s\n", color_java, cpu,
	    pid, tid, this->delta, "method", self->depth[arg0] * 2, "",
	    stringof(this->class), stringof(this->method), color_off);
	self->depth[arg0]++;
	self->depthlast = self->depth[arg0];
	self->last = timestamp;
}

hotspot$target:::method-return
{
	this->delta = (timestamp - self->last) / 1000;
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';

	self->depth[arg0] -= self->depth[arg0] > 0 ? 1 : 0;
	printf("%s%3d %6d/%-5d %9d %-8s %*s<- %s.%s%s\n", color_java, cpu,
	    pid, tid, this->delta, "method", self->depth[arg0] * 2, "",
	    stringof(this->class), stringof(this->method), color_off);
	self->depthlast = self->depth[arg0];
	self->last = timestamp;
}

syscall:::entry
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d/%-5d %9d %-8s %*s-> %s%s\n", color_syscall,
	    cpu, pid, tid, this->delta, "syscall", self->depthlast * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

syscall:::return
/pid == $target/
{
	this->delta = (timestamp - self->last) / 1000;
	printf("%s%3d %6d/%-5d %9d %-8s %*s<- %s%s\n", color_syscall,
	    cpu, pid, tid, this->delta, "syscall", self->depthlast * 2, "",
	    probefunc, color_off);
	self->last = timestamp;
}

proc:::exit
/pid == $target/
{
	exit(0);
}
