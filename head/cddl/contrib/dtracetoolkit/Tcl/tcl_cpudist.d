#!/usr/sbin/dtrace -CZs
/*
 * tcl_cpudist.d - measure Tcl on-CPU time for different types of operation.
 *                 Written for the Tcl DTrace provider.
 *
 * $Id: tcl_cpudist.d 63 2007-10-04 04:34:38Z brendan $
 *
 * USAGE: tcl_cpudist.d [top]	# hit Ctrl-C to end
 *    eg,
 *        tcl_cpudist.d		# default, truncate to 10 lines
 *        tcl_cpudist.d 25	# truncate each report section to 25 lines
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * FIELDS:
 *		1		Process ID
 *		2		Type of call (proc/cmd/total)
 *		3		Name of call
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

#define TOP	10		/* default output truncation */
#define B_FALSE	0

#pragma D option quiet
#pragma D option defaultargs

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
	top = $1 != 0 ? $1 : TOP;
}

tcl*:::proc-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->proc[self->depth] = vtimestamp;
}

tcl*:::proc-return
/self->proc[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->proc[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->proc[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->name = copyinstr(arg0);

	@types_incl[pid, "proc", this->name] =
	    quantize(this->oncpu_incl / 1000);
	@types_excl[pid, "proc", this->name] =
	    quantize(this->oncpu_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

tcl*:::cmd-entry
{
	self->depth++;
	self->exclude[self->depth] = 0;
	self->cmd[self->depth] = vtimestamp;
}

tcl*:::cmd-return
/self->cmd[self->depth]/
{
	this->oncpu_incl = vtimestamp - self->cmd[self->depth];
	this->oncpu_excl = this->oncpu_incl - self->exclude[self->depth];
	self->cmd[self->depth] = 0;
	self->exclude[self->depth] = 0;
	this->name = copyinstr(arg0);

	@types_incl[pid, "cmd", this->name] =
	    quantize(this->oncpu_incl / 1000);
	@types_excl[pid, "cmd", this->name] =
	    quantize(this->oncpu_excl / 1000);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

dtrace:::END
{
	trunc(@types_excl, top);
	printf("\nTop %d exclusive on-CPU times (us),\n", top);
	printa("   PID=%d, %s, %s %@d\n", @types_excl);

	trunc(@types_incl, top);
	printf("\nTop %d inclusive on-CPU times (us),\n", top);
	printa("   PID=%d, %s, %s %@d\n", @types_incl);
}
