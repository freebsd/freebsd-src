#!/usr/sbin/dtrace -s
/*
 * sysbypid.d - print sysinfo events by process.
 *		Uses DTrace (Solaris 10 3/05).
 *
 * $Id: sysbypid.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE: sysbypid.d
 *
 * FIELDS:
 *		EXEC	Process name
 *		PID	Process ID
 * 		SYS	System statistic (see /usr/include/sys/sysinfo.h)
 *		VALUE	Value by which statistic was incremented
 *
 * The virtual memory statistics are documented in the cpu_sysinfo struct
 * in the /usr/include/sys/sysinfo.h file; and also in the sysinfo provider
 * chapter of the DTrace Guide, http://docs.sun.com/db/doc/817-6223.
 *
 * COPYRIGHT: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 14-May-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN {
	printf("Tracing... Hit Ctrl-C to end.\n");
}

sysinfo::: {
	@Sys[execname, pid, probename] = sum(arg0);
}

dtrace:::END {
	printf("%16s %8s %22s %8s\n", "EXEC", "PID", "SYS", "VALUE");
	printa("%16s %8d %22s %@8d\n", @Sys);
}
