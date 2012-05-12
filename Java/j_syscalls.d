#!/usr/sbin/dtrace -Zs
/*
 * j_syscalls.d - count Java methods and syscalls using DTrace.
 *                Written for the Java hotspot DTrace provider.
 *
 * $Id: j_syscalls.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This traces Java methods if the hotspot provider exists (1.6.0) and
 * the flag "+ExtendedDTraceProbes" is used. eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_syscalls.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		TYPE		Type of call (method/syscall)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
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

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

hotspot$target:::method-entry
{
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';
	this->name = strjoin(strjoin(stringof(this->class), "."),
	    stringof(this->method));
	@calls[pid, "method", this->name] = count();
}

syscall:::entry
/pid == $target/
{
	@calls[pid, "syscall", probefunc] = count();
}


dtrace:::END
{
	printf(" %6s %-8s %-52s %8s\n", "PID", "TYPE", "NAME", "COUNT");
	printa(" %6d %-8s %-52s %@8d\n", @calls);
}
