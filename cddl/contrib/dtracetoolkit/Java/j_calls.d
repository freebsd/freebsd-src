#!/usr/sbin/dtrace -Zs
/*
 * j_calls.d - count Java calls (method/...) using DTrace.
 *             Written for the Java hotspot DTrace provider.
 *
 * $Id: j_calls.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0). Method calls and object allocation are only
 * visible when using the flag "+ExtendedDTraceProbes". eg,
 * java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_calls.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		TYPE		Type of call (see below)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
 *
 * TYPEs:
 *		cload		class load
 *		method		method call
 *		mcompile	method compile
 *		mload		compiled method load
 *		oalloc		object alloc
 *		thread		thread start
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
	@calls[pid, "method", this->name] = count();
}

hotspot*:::object-alloc
{
	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	@calls[pid, "oalloc", stringof(this->class)] = count();
}

hotspot*:::class-loaded
{
	this->class = (char *)copyin(arg0, arg1 + 1);
	this->class[arg1] = '\0';
	@calls[pid, "cload", stringof(this->class)] = count();
}

hotspot*:::thread-start
{
	this->thread = (char *)copyin(arg0, arg1 + 1);
	this->thread[arg1] = '\0';
	@calls[pid, "thread", stringof(this->thread)] = count();
}

hotspot*:::method-compile-begin
{
	this->class = (char *)copyin(arg0, arg1 + 1);
	this->class[arg1] = '\0';
	this->method = (char *)copyin(arg2, arg3 + 1);
	this->method[arg3] = '\0';
	this->name = strjoin(strjoin(stringof(this->class), "."),
	    stringof(this->method));
	@calls[pid, "mcompile", this->name] = count();
}

hotspot*:::compiled-method-load
{
	this->class = (char *)copyin(arg0, arg1 + 1);
	this->class[arg1] = '\0';
	this->method = (char *)copyin(arg2, arg3 + 1);
	this->method[arg3] = '\0';
	this->name = strjoin(strjoin(stringof(this->class), "."),
	    stringof(this->method));
	@calls[pid, "mload", this->name] = count();
}

dtrace:::END
{
	printf(" %6s %-8s %-52s %8s\n", "PID", "TYPE", "NAME", "COUNT");
	printa(" %6d %-8s %-52s %@8d\n", @calls);
}
