#!/usr/sbin/dtrace -Zs
/*
 * js_calldist.d - measure JavaScript elapsed times for types of operation.
 *                 Written for the JavaScript DTrace provider.
 *
 * $Id: js_calldist.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces JavaScript activity from all browsers running on the system with
 * JavaScript provider support.
 *
 * USAGE: js_calldist.d 	# hit Ctrl-C to end
 *
 * FIELDS:
 *		1		Filename of the JavaScript program
 *		2		Type of call (func/obj-new)
 *		3		Name of call
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
	self->function[self->depth] = timestamp;
}

javascript*:::function-return
/self->function[self->depth]/
{
	this->elapsed_incl = timestamp - self->function[self->depth];
	this->elapsed_excl = this->elapsed_incl - self->exclude[self->depth];
	self->function[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg2);

	@types_incl[this->file, "func", this->name] =
	    quantize(this->elapsed_incl / 1000);
	@types_excl[this->file, "func", this->name] =
	    quantize(this->elapsed_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->elapsed_incl;
}

javascript*:::object-create-start
{
	self->object = timestamp;
}

javascript*:::object-create-done
/self->object/
{
	this->elapsed = timestamp - self->object;
	self->object = 0;
	this->file = basename(copyinstr(arg0));
	this->name = copyinstr(arg1);

	@types[this->file, "obj-new", this->name] =
	    quantize(this->elapsed / 1000);

	self->exclude[self->depth] += this->elapsed;
}

dtrace:::END
{
	printf("\nElapsed times (us),\n");
	printa("   %s, %s, %s %@d\n", @types);

	printf("\nExclusive function elapsed times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("\nInclusive function elapsed times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
