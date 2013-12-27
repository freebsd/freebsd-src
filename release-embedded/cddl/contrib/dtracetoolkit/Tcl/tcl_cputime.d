#!/usr/sbin/dtrace -CZs
/*
 * tcl_cputime.d - measure Tcl on-CPU times for different types of operation.
 *                 Written for the Tcl DTrace provider.
 *
 * $Id: tcl_cputime.d 63 2007-10-04 04:34:38Z brendan $
 *
 * USAGE: tcl_cputime.d [top]	# hit Ctrl-C to end
 *    eg,
 *        tcl_cputime.d		# default, truncate to 10 lines
 *        tcl_cputime.d 25	# truncate each report section to 25 lines
 *
 * This traces activity from all Tcl processes on the system with DTrace
 * provider support (tcl8.4.16).
 *
 * FIELDS:
 *		PID		Process ID
 *		TYPE		Type of call (proc/cmd/total)
 *		NAME		Name of call
 *		TOTAL		Total on-CPU time for calls (us)
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

	@num[pid, "proc", this->name] = count();
	@num[0, "total", "-"] = count();
	@types_incl[pid, "proc", this->name] = sum(this->oncpu_incl);
	@types_excl[pid, "proc", this->name] = sum(this->oncpu_excl);
	@types_excl[0, "total", "-"] = sum(this->oncpu_excl);

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

	@num[pid, "cmd", this->name] = count();
	@num[0, "total", "-"] = count();
	@types_incl[pid, "cmd", this->name] = sum(this->oncpu_incl);
	@types_excl[pid, "cmd", this->name] = sum(this->oncpu_excl);
	@types_excl[0, "total", "-"] = sum(this->oncpu_excl);

	self->depth--;
	self->exclude[self->depth] += this->oncpu_incl;
}

dtrace:::END
{
	trunc(@num, top);
	printf("\nTop %d counts,\n", top);
	printf("   %6s %-10s %-48s %8s\n", "PID", "TYPE", "NAME", "COUNT");
	printa("   %6d %-10s %-48s %@8d\n", @num);

	trunc(@types_excl, top);
	normalize(@types_excl, 1000);
	printf("\nTop %d exclusive on-CPU times (us),\n", top);
	printf("   %6s %-10s %-48s %8s\n", "PID", "TYPE", "NAME", "TOTAL");
	printa("   %6d %-10s %-48s %@8d\n", @types_excl);

	trunc(@types_incl, top);
	normalize(@types_incl, 1000);
	printf("\nTop %d inclusive on-CPU times (us),\n", top);
	printf("   %6s %-10s %-48s %8s\n", "PID", "TYPE", "NAME", "TOTAL");
	printa("   %6d %-10s %-48s %@8d\n", @types_incl);
}
