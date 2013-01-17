#!/usr/sbin/dtrace -Zs
/*
 * py_calldist.d - measure Python elapsed times for functions.
 *                 Written for the Python DTrace provider.
 *
 * $Id: py_calldist.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces Python activity from all programs running on the system with
 * Python provider support.
 *
 * USAGE: py_calldist.d 	# hit Ctrl-C to end
 *
 * This script prints distribution plots of elapsed time for Python
 * operations. Use py_calltime.d for summary reports.
 *
 * FIELDS:
 *		1		Filename of the Python program
 *		2		Type of call (func)
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

python*:::function-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->function[self->depth] = timestamp;
}

python*:::function-return
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

dtrace:::END
{
	printf("\nExclusive function elapsed times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("\nInclusive function elapsed times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
