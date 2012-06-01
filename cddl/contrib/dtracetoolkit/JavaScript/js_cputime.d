#!/usr/sbin/dtrace -Zs
/*
 * js_cputime.d - measure JavaScript on-CPU times for types of operation.
 *                Written for the JavaScript DTrace provider.
 *
 * $Id: js_cputime.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces JavaScript activity from all browsers running on the system with
 * JavaScript provider support.
 *
 * USAGE: js_cputime.d	 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the JavaScript program
 *		TYPE		Type of call (func/obj-new/gc/total)
 *		NAME		Name of call
 *		TOTAL		Total on-CPU time for calls (us)
 *
 * Filename and function names are printed if available.
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

javascript*:::function-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->function[self->depth] = vtimestamp;
}

javascript*:::function-return
/self->function[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->function[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->function[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg2);

	@num[this->file, "func", this->name] = count();
	@num["-", "total", "-"] = count();
	@types_incl[this->file, "func", this->name] = sum(this->oncpu_incl);
	@types_excl[this->file, "func", this->name] = sum(this->oncpu_excl);
	@types_excl["-", "total", "-"] = sum(this->oncpu_excl);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

javascript*:::object-create-start
{
	self->object = vtimestamp;
}

javascript*:::object-create-done
/self->object/
{
	this->oncpu = vtimestamp - self->object;
	self->object = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@num[this->file, "obj-new", this->name] = count();
	@num["-", "total", "-"] = count();
	@types[this->file, "obj-new", this->name] = sum(this->oncpu);
	@types["-", "total", "-"] = sum(this->oncpu);

	self->exclude[self->depth] += this->oncpu;
}

dtrace:::END
{
	printf("\nCount,\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "COUNT");
	printa("   %-20.20s %-10s %-32s %@8d\n", @num);

	normalize(@types, 1000);
	printf("\nElapsed times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20.20s %-10s %-32s %@8d\n", @types);

	normalize(@types_excl, 1000);
	printf("\nExclusive function on-CPU times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20.20s %-10s %-32s %@8d\n", @types_excl);

	normalize(@types_incl, 1000);
	printf("\nInclusive function on-CPU times (us),\n");
	printf("   %-20s %-10s %-32s %8s\n", "FILE", "TYPE", "NAME", "TOTAL");
	printa("   %-20.20s %-10s %-32s %@8d\n", @types_incl);
}
