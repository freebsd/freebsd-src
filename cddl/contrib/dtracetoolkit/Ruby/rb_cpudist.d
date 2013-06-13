#!/usr/sbin/dtrace -Zs
/*
 * rb_cpudist.d - measure Ruby on-CPU times for types of operation.
 *                Written for the Ruby DTrace provider.
 *
 * $Id: rb_cpudist.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces Ruby activity from all programs running on the system with
 * Ruby provider support.
 *
 * USAGE: rb_cpudist.d 		# hit Ctrl-C to end
 *
 * This script prints distribution plots of elapsed time for Ruby
 * operations. Use rb_cputime.d for summary reports.
 *
 * FIELDS:
 *		1		Filename of the Ruby program
 *		2		Type of call (method/obj-new/gc)
 *		3		Name of call
 *
 * Filename and method names are printed if available.
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

ruby*:::function-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->function[self->depth] = vtimestamp;
}

ruby*:::function-return
/self->function[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->function[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->function[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg2));
	this->name = strjoin(strjoin(copyinstr(arg0), "::"), copyinstr(arg1));

	@types_incl[this->file, "func", this->name] =
	    quantize(this->oncpu_incl / 1000);
	@types_excl[this->file, "func", this->name] =
	    quantize(this->oncpu_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

ruby*:::object-create-start
{
	self->object = vtimestamp;
}

ruby*:::object-create-done
/self->object/
{
	this->oncpu = vtimestamp - self->object;
	self->object = 0;
	this->file = basename(copyinstr(arg1));
	this->file = this->file != NULL ? this->file : ".";

	@types[this->file, "obj-new", copyinstr(arg0)] =
	    quantize(this->oncpu / 1000);

	self->exclude[self->depth] += this->oncpu;
}

ruby*:::gc-begin
{
	self->gc = vtimestamp;
}

ruby*:::gc-end
/self->gc/
{
	this->oncpu = vtimestamp - self->gc;
	self->gc = 0;

	@types[".", "gc", "-"] = quantize(this->oncpu / 1000);

	self->exclude[self->depth] += this->oncpu;
}

dtrace:::END
{
	printf("\nOn-CPU times (us),\n");
	printa("   %s, %s, %s %@d\n", @types);

	printf("\nExclusive function on-CPU times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("\nInclusive function on-CPU times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
