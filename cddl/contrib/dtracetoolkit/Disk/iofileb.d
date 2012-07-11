#!/usr/sbin/dtrace -s
/*
 * iofileb.d - I/O bytes by filename and process.
 *             Written using DTrace (Solaris 10 3/05).
 *
 * This prints a summary of requested disk activity by pathname,
 * providing totals of the I/O events in bytes. It is a companion to the
 * iofile.d script - which prints in terms of I/O wait time, not bytes.
 * I/O wait time is a better metric for understanding performance issues.
 * Both disk and NFS I/O are measured.
 *
 * $Id: iofileb.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	iofileb.d	# wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID	process ID
 *		CMD	command name
 *		KB	Kilobytes of disk I/O
 *		FILE	Full pathname of the file
 *
 * COPYRIGHT: Copyright (c) 2006 Brendan Gregg.
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
 * 20-Feb-2006	Brendan Gregg	Created this.
 * 20-Feb-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

io:::start
{
	@files[pid, execname, args[2]->fi_pathname] = sum(args[0]->b_bcount);
}

dtrace:::END
{
	normalize(@files, 1024);
	printf("%6s %-12s %6s %s\n", "PID", "CMD", "KB", "FILE");
	printa("%6d %-12.12s %@6d %s\n", @files);
}
