#!/usr/sbin/dtrace -Zs
/*
 * j_methodcalls.d - count Java method calls DTrace.
 *                   Written for the Java hotspot DTrace provider.
 *
 * $Id: j_methodcalls.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0) and the flag "+ExtendedDTraceProbes". eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_methodcalls.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		COUNT		Number of calls during sample
 *		CLASS.METHOD	Java class and method name
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

hotspot*:::method-entry
{
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';
	this->name = strjoin(strjoin(stringof(this->class), "."),
	    stringof(this->method));
	@calls[pid, this->name] = count();
}

dtrace:::END
{
	printf(" %6s %8s %s\n", "PID", "COUNT", "CLASS.METHOD");
	printa(" %6d %@8d %s\n", @calls);
}
