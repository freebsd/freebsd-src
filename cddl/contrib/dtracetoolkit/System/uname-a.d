#!/usr/sbin/dtrace -s
/*
 * uname-a.d - "uname -a" demo in DTrace.
 *             Written using DTrace (Solaris 10 3/05).
 *
 * This has been written to demonstrate fetching the "uname -a" info
 * from a DTrace script, which turns out to be all kernel variables.
 * This is intended as a starting point for other DTrace scripts, by
 * beginning with familiar statistics.
 *
 * $Id: uname-a.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	uname-a.d
 *
 * FIELDS:	See uname(1) manpage for documentation.
 *
 * SEE ALSO:	uname
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
 * 24-Jul-2005	Brendan Gregg	Created this.
 * 24-Jul-2005	   "      "	Last update.
 */

#pragma D option quiet
#pragma D option bufsize=8k

/* print system info */
dtrace:::BEGIN
{
	printf("%s %s %s %s %s %s %s",
	    `utsname.sysname,
	    `utsname.nodename,
	    `utsname.release,
	    `utsname.version,
	    `utsname.machine,
	    `architecture,
	    `platform);

	exit(0);
}
