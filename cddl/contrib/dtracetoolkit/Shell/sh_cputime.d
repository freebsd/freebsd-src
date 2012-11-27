#!/usr/sbin/dtrace -Zs
/*
 * sh_cputime.d - measure Bourne shell on-CPU times for types of operation.
 *                Written for the sh DTrace provider.
 *
 * $Id: sh_cputime.d 46 2007-09-17 10:25:36Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_cputime.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the shell or shellscript
 *		TYPE		Type of call (func/builtin/cmd/total)
 *		NAME		Name of call
 *		TOTAL		Total on-CPU time for calls (us)
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
	printf("Tracing... Hit Ctrl-C to end.\n");
}

sh*:::function-entry
{
	self->depth++;
	self->function[self->depth] = vtimestamp;
	self->exclude[self->depth] = 0;
}

sh*:::function-return
/self->function[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->function[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->function[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@num[this->file, "func", this->name] = count();
	@num["-", "total", "-"] = count();
	@types_incl[this->file, "func", this->name] = sum(this->oncpu_incl);
	@types_excl[this->file, "func", this->name] = sum(this->oncpu_excl);
	@types_excl["-", "total", "-"] = sum(this->oncpu_excl);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

sh*:::builtin-entry
{
	self->builtin = vtimestamp;
}

sh*:::builtin-return
/self->builtin/
{
	this->oncpu = vtimestamp - self->builtin;
	self->builtin = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@num[this->file, "builtin", this->name] = count();
	@num["-", "total", "-"] = count();
	@types[this->file, "builtin", this->name] = sum(this->oncpu);
	@types["-", "total", "-"] = sum(this->oncpu);

	self->exclude[self->depth] += this->oncpu;
}

sh*:::command-entry
{
	incmd[pid] = basename(copyinstr(arg0));
	depth[pid] = self->depth;
}

sh*:::command-return
{
	incmd[pid] = 0;
}

proc:::exec-success
{
	/*
	 * Due to thread timing after fork(), this probe can fire before
	 * sh*:::command-entry has, which means we can't predicate this
	 * exec() away just yet. Store the vtimestamp in case it is needed.
	 */
	self->command = vtimestamp;
}

proc:::exit
/incmd[ppid] == NULL/
{
	self->command = 0;
}

proc:::exit
/incmd[ppid] != NULL/
{
	this->oncpu = vtimestamp - self->command;
	self->command = 0;

	@num[incmd[ppid], "cmd", execname] = count();
	@num["-", "total", "-"] = count();
	@types[incmd[ppid], "cmd", execname] = sum(this->oncpu);
	@types["-", "total", "-"] = sum(this->oncpu);

	self->exclude[depth[ppid]] += this->oncpu;
	incmd[ppid] = 0;
	depth[ppid] = 0;
}

dtrace:::END
{
	printf("\nCounts,\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "COUNT");
	printa("   %-20s %-10s %-32s %@8d\n", @num);

	normalize(@types, 1000);
	printf("\nOn-CPU times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types);

	normalize(@types_excl, 1000);
	printf("\nExclusive function on-CPU times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types_excl);

	normalize(@types_incl, 1000);
	printf("\nInclusive function on-CPU times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types_incl);
}
