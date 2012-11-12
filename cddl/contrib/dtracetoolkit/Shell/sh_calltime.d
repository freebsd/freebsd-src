#!/usr/sbin/dtrace -Zs
/*
 * sh_calltime.d - measure Bourne shell elapsed times for types of operation.
 *                 Written for the sh DTrace provider.
 *
 * $Id: sh_calltime.d 46 2007-09-17 10:25:36Z brendan $
 *
 * This traces shell activity from all Bourne shells on the system that are
 * running with sh provider support.
 *
 * USAGE: sh_calltime.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the shell or shellscript
 *		TYPE		Type of call (func/builtin/cmd/total)
 *		NAME		Name of call
 *		TOTAL		Total elapsed time for calls (us)
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

	@num[this->file, "func", this->name] = count();
	@num["-", "total", "-"] = count();
	@types_incl[this->file, "func", this->name] = sum(this->elapsed_incl);
	@types_excl[this->file, "func", this->name] = sum(this->elapsed_excl);
	@types_excl["-", "total", "-"] = sum(this->elapsed_excl);

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
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@num[this->file, "builtin", this->name] = count();
	@num["-", "total", "-"] = count();
	@types[this->file, "builtin", this->name] = sum(this->elapsed);
	@types["-", "total", "-"] = sum(this->elapsed);

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
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@num[this->file, "cmd", this->name] = count();
	@num["-", "total", "-"] = count();
	@types[this->file, "cmd", this->name] = sum(this->elapsed);
	@types["-", "total", "-"] = sum(this->elapsed);

	self->exclude[self->depth] += this->elapsed;
}

dtrace:::END
{
	printf("\nCounts,\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "COUNT");
	printa("   %-20s %-10s %-32s %@8d\n", @num);

	normalize(@types, 1000);
	printf("\nElapsed times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types);

	normalize(@types_excl, 1000);
	printf("\nExclusive function elapsed times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types_excl);

	normalize(@types_incl, 1000);
	printf("\nInclusive function elapsed times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20s %-10s %-32s %@8d\n", @types_incl);
}
