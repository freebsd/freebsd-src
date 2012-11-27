#!/usr/sbin/dtrace -s
/*
 * tcpwdist.d - simple TCP write distribution by process.
 *              Written in DTrace (Solaris 10 3/05).
 *
 * This measures the size of writes from applications to the TCP level, which
 * may well be much larger than the MTU size (this is application writes not
 * packet writes). It can help identify which process is creating network
 * traffic, and the size of the writes by that application. It uses a simple
 * probe that produces meaningful output for most protocols.
 *
 * Tracking TCP activity by process is complex for a number of reasons,
 * the greatest is that inbound TCP traffic is asynchronous to the process.
 * The easiest TCP traffic to match is writes, which this script demonstrates.
 * However there are still issues - for an inbound telnet connection the
 * writes are associated with the command, for example "ls -l", not something
 * meaningful such as "in.telnetd".
 *
 * Scripts that match TCP traffic properly include tcpsnoop and tcptop.
 *
 * $Id: tcpwdist.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       tcpwdist.d          # wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID	process ID
 *		CMD	command and argument list
 *		value	TCP write payload size in bytes
 *		count	number of writes
 *
 * SEE ALSO:	tcpsnoop, tcptop
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
 * 09-Jul-2004	Brendan Gregg	Created this.
 * 14-Jun-2005	   "      "	Rewrote this as tcpwdist.d.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

/*
 * Process TCP Write
 */
fbt:ip:tcp_output:entry
{
	/* fetch details */
	this->size = msgdsize(args[1]);

	/* store details */
	@Size[pid, curpsinfo->pr_psargs] = quantize(this->size);
}

/*
 * Print final report
 */
dtrace:::END
{
	printa(" PID: %-6d CMD: %S\n%@d\n", @Size);
}
