#!/usr/sbin/dtrace -Zs
/*
 * sh_syscalls.d - count Bourne calls and syscalls using DTrace.
 *                 Written for the sh DTrace provider.
 *
 * $Id: sh_syscalls.d 25 2007-09-12 09:51:58Z brendan $
 *
 * USAGE: sh_syscalls.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the shell or shellscript
 *		TYPE		Type of call (func/builtin/cmd/syscall)
 *		NAME		Name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and function names are printed if available.
 * The filename for syscalls may be printed as the shell name, if the
 * script was invoked using the form "shell filename" rather than running
 * the script with an interpreter line.
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

sh$target:::function-entry,
sh$target:::builtin-entry,
sh$target:::command-entry
/self->filename == NULL/
{
	self->filename = basename(copyinstr(arg0));
}

sh$target:::function-entry
{
	@calls[self->filename, "func", copyinstr(arg1)] = count();
}

sh$target:::builtin-entry
{
	@calls[self->filename, "builtin", copyinstr(arg1)] = count();
}

sh$target:::command-entry
{
	@calls[self->filename, "cmd", copyinstr(arg1)] = count();
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
