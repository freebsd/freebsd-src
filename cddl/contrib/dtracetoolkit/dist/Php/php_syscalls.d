#!/usr/sbin/dtrace -Zs
/*
 * php_syscalls.d - count PHP function calls and syscalls using DTrace.
 *                  Written for the PHP DTrace provider.
 *
 * This traces syscalls that occured during a PHP function call.
 *
 * $Id: php_syscalls.d 53 2007-09-24 04:58:38Z brendan $
 *
 * USAGE: php_syscalls.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		FILE		Filename of the PHP program
 *		TYPE		Type of call (func/syscall)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and function names are printed if available.
 * The filename for syscalls may be printed as "php", if the program
 * was invoked using the form "php filename" rather than running the
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

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

php*:::function-entry
/arg0/
{
	@calls[pid, basename(copyinstr(arg1)), "func", copyinstr(arg0)] =
	    count();
	self->php++;
}

php*:::function-return
/arg0/
{
	self->php -= self->php == 0 ? 0 : 1;
}

syscall:::entry
/self->php > 0/
{
	@calls[pid, basename(execname), "syscall", probefunc] = count();
}

dtrace:::END
{
	printf(" %-6s  %-26s %-10s %-22s %8s\n", "PID", "FILE", "TYPE", "NAME",
	    "COUNT");
	printa(" %-6d  %-26s %-10s %-22s %@8d\n", @calls);
}
