#!/usr/sbin/dtrace -Zs
/*
 * j_package.d - count Java class loads by package using DTrace.
 *               Written for the Java hotspot DTrace provider.
 *
 * $Id: j_package.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0).
 *
 * USAGE: j_package.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		LOADS		Class loads during trace
 *		PACKAGE		Package name from class
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

hotspot*:::class-loaded
{
	this->class = (char *)copyin(arg0, arg1 + 1);
	this->class[arg1] = '\0';

	@loads[pid, dirname(stringof(this->class))] = count();
}

dtrace:::END
{
	printf("   %6s %8s  %s\n", "PID", "LOADS", "PACKAGE");
	printa("   %6d %@8d  %s\n", @loads);
}
