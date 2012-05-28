#!/usr/sbin/dtrace -Zs
/*
 * sh_wasted.d - measure Bourne shell elapsed times for "wasted" commands.
 *               Written for the sh DTrace provider.
 *
 * $Id: sh_wasted.d 25 2007-09-12 09:51:58Z brendan $
 *
 * USAGE: sh_wasted.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * This script measures "wasted" commands - those which are called externally
 * but are in fact builtins to the shell. Ever seen a script which calls
 * /usr/bin/echo needlessly? This script measures that cost.
 *
 * FIELDS:
 *		FILE		Filename of the shell or shellscript
 *		NAME		Name of call
 *		TIME		Total elapsed time for calls (us)
 *
 * IDEA: Mike Shapiro
 *
 * Filename and call names are printed if available.
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
	isbuiltin["echo"] = 1;
	isbuiltin["test"] = 1;
	/* add builtins here */

	printf("Tracing... Hit Ctrl-C to end.\n");
	self->start = timestamp;
}

sh$target:::command-entry
{
	self->command = timestamp;
}

sh$target:::command-return
{
	this->elapsed = timestamp - self->command;
	this->path = copyinstr(arg1);
	this->cmd = basename(this->path);
}

sh$target:::command-return
/self->command && !isbuiltin[this->cmd]/
{
	@types_cmd[basename(copyinstr(arg0)), this->path] = sum(this->elapsed);
	self->command = 0;
}

sh$target:::command-return
/self->command/
{
	@types_wasted[basename(copyinstr(arg0)), this->path] =
	    sum(this->elapsed);
	self->command = 0;
}

proc:::exit
/pid == $target/
{
	exit(0);
}

dtrace:::END
{
	this->elapsed = (timestamp - self->start) / 1000;
	printf("Script duration: %d us\n", this->elapsed);

	normalize(@types_cmd, 1000);
	printf("\nExternal command elapsed times,\n");
	printf("   %-30s %-22s %8s\n", "FILE", "NAME", "TIME(us)");
	printa("   %-30s %-22s %@8d\n", @types_cmd);

	normalize(@types_wasted, 1000);
	printf("\nWasted command elapsed times,\n");
	printf("   %-30s %-22s %8s\n", "FILE", "NAME", "TIME(us)");
	printa("   %-30s %-22s %@8d\n", @types_wasted);
}
