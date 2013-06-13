#!/usr/sbin/dtrace -Zs
/*
 * pl_cpudist.d - measure Perl on-CPU times for subroutines.
 *                Written for the Perl DTrace provider.
 *
 * $Id: pl_cpudist.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces Perl activity from all programs running on the system with
 * Perl provider support.
 *
 * USAGE: pl_cpudist.d 		# hit Ctrl-C to end
 *
 * This script prints distribution plots of elapsed time for Perl subrotines.
 * Use pl_cputime.d for summary reports.
 *
 * FIELDS:
 *		1		Filename of the Perl program
 *		2		Type of call (sub)
 *		3		Name of call
 *
 * Filename and subroutine names are printed if available.
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

perl*:::sub-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->sub[self->depth] = vtimestamp;
}

perl*:::sub-return
/self->sub[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->sub[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->sub[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->file = basename(copyinstr(arg1));
	this->name = copyinstr(arg0);

	@types_incl[this->file, "sub", this->name] =
	    quantize(this->oncpu_incl / 1000);
	@types_excl[this->file, "sub", this->name] =
	    quantize(this->oncpu_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

dtrace:::END
{
	printf("\nExclusive subroutine on-CPU times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_excl);

	printf("\nInclusive subroutine on-CPU times (us),\n");
	printa("   %s, %s, %s %@d\n", @types_incl);
}
