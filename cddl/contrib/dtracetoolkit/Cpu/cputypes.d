#!/usr/sbin/dtrace -s
/*
 * cputypes.d - list CPU type info.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * $Id: cputypes.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	cputypes.d
 *
 * FIELDS:
 *		CPU		CPU ID
 *		CHIP		chip ID
 *		PSET		processor set ID
 *		LGRP		latency group ID
 *		CLOCK		clock speed, MHz
 *		TYPE		CPU type
 *		FPU		floating point identifier types
 *
 * SEE ALSO:	psrinfo(1M)
 *		/usr/include/sys/processor.h
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
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
 * 27-Jun-2005  Brendan Gregg   Created this.
 * 27-Jun-2005     "      "	Last update.
 */

#pragma D option quiet
#pragma D option bufsize=64k

dtrace:::BEGIN
{
	printf("%4s %4s %4s %4s %6s  %-16s %s\n",
	    "CPU", "CHIP", "PSET", "LGRP", "CLOCK", "TYPE", "FPU");
	done[0] = 0;
}

profile:::profile-10ms
/done[cpu] == 0/
{
	printf("%4d %4d %4d %4d %6d  %-16s %s\n",
	    cpu, curcpu->cpu_chip, curcpu->cpu_pset,
	    curcpu->cpu_lgrp, curcpu->cpu_info.pi_clock,
	    stringof(curcpu->cpu_info.pi_processor_type),
	    stringof(curcpu->cpu_info.pi_fputypes));
	done[cpu]++;
}

profile:::tick-100ms
{
	exit(0);
}
