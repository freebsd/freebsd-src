#!/usr/sbin/dtrace -Zs
/*
 * sh_calldist.d - measure Bourne shell elapsed times for types of operation.
 *                 Written for the sh DTrace provider.
 *
 * $Id: sh_calldist.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_calldist.d 	# hit Ctrl-C to end
 *
 * This script prints distribution plots of elapsed time for shell
 * operations. Use sh_calltime.d for summary reports.
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
	self->exclude[self->depth] = 0;
	self->function[self->depth] = timestamp;
}

sh*:::function-return
/self->function[self->depth]/
{
	this->elapsed_incl = timestamp - self->function[self->depth];
	this->elapsed_excl = this->elapsed_incl - self->exclude[self->depth];
	self->function[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@types_incl[this->file, "func", this->name] =
	    quantize(this->elapsed_incl / 1000);
	@types_excl[this->file, "func", this->name] =
	    quantize(this->elapsed_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->elapsed_incl;
}

sh*:::builtin-entry
{
	self->builtin = timestamp;
}

sh*:::builtin-return
/self->builtin/
{
	this->elapsed = timestamp - self->builtin;
	self->builtin = 0;

	@types[basename(copyinstr(arg0)), "builtin", copyinstr(arg1)] =
	    quantize(this->elapsed / 1000);

	self->exclude[self->depth] += this->elapsed;
}

sh*:::command-entry
{
	self->command = timestamp;
}

sh*:::command-return
/self->command/
{
	this->elapsed = timestamp - self->command;
	self->command = 0;

	@types[basename(copyinstr(arg0)), "cmd", copyinstr(arg1)] =
	    quantize(this->elapsed / 1000);

	self->exclude[self->depth] += this->elapsed;
}

dtrace:::END
{
	printf("Elapsed times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types);

	printf("Exclusive function elapsed times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("Inclusive function elapsed times (us),\n\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
