#!/usr/sbin/dtrace -Zs
/*
 * rb_syscalls.d - count Ruby calls and syscalls using DTrace.
 *                 Written for the Ruby DTrace provider.
 *
 * $Id: rb_syscalls.d 20 2007-09-12 09:28:22Z brendan $
 *
 * USAGE: rb_syscalls.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the Ruby program
 *		TYPE		Type of call (method/syscall)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and method names are printed if available.
 * The filename for syscalls may be printed as "ruby", if the program
 * was invoked using the form "ruby filename" rather than running the
 * program with an interpreter line.
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

self string filename;

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

ruby$target:::function-entry
{
	this->name = strjoin(strjoin(copyinstr(arg0), "::"), copyinstr(arg1));
	@calls[basename(copyinstr(arg2)), "method", this->name] = count();
}

syscall:::entry
/pid == $target/
{
	@calls[basename(execname), "syscall", probefunc] = count();
}

dtrace:::END
{
	printf("\nCalls for PID %d,\n\n", $target);
	printf(" %-32s %-10s %-22s %8s\n", "FILE", "TYPE", "NAME", "COUNT");
	printa(" %-32s %-10s %-22s %@8d\n", @calls);
}
