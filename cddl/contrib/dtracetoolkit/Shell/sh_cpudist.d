#!/usr/sbin/dtrace -Zs
/*
 * sh_cpudist.d - measure Bourne shell on-CPU times for types of operation.
 *                Written for the sh DTrace provider.
 *
 * $Id: sh_cpudist.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_cpudist.d 		# hit Ctrl-C to end
 *
 * This script prints distribution plots of on-CPU time for shell
 * operations. Use sh_cputime.d for summary reports.
 *
 * FIELDS:
 *		1		Filename of the shell or shellscript
 *		2		Type of call (func/builtin/cmd)
 *		3		Name of call
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

	@types_incl[this->file, "func", this->name] =
	    quantize(this->oncpu_incl / 1000);
	@types_excl[this->file, "func", this->name] =
	    quantize(this->oncpu_excl / 1000);

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

	@types[basename(copyinstr(arg0)), "builtin", copyinstr(arg1)] =
	    quantize(this->oncpu / 1000);

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

	@types[incmd[ppid], "cmd", execname] = quantize(this->oncpu / 1000);

	self->exclude[depth[ppid]] += this->oncpu;
	incmd[ppid] = 0;
	depth[ppid] = 0;
}

dtrace:::END
{
	printf("On-CPU times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types);

	printf("Exclusive function on-CPU times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("Inclusive function on-CPU times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
