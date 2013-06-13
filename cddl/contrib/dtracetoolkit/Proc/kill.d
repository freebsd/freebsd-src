#!/usr/sbin/dtrace -qs
/*
 * kill.d - watch process signals as they are sent (eg, kill -9).
 *          Written in DTrace (Solaris 10 3/05).
 *
 * $Id: kill.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       kill.d
 *
 * FIELDS:
 *              FROM     source PID
 *              COMMAND  source command name
 *              TO       destination PID
 *              SIG      destination signal ("9" for a kill -9)
 *              RESULT   result of signal (-1 is for failure)
 *
 * SEE ALSO: Chapter 25, Solaris Dynamic Tracing Guide, docs.sun.com,
 *           for a solution using proc:::signal-send.
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
 * 09-May-2004  Brendan Gregg   Created this.
 * 28-Jun-2005	   "      "	Last update.
 */

dtrace:::BEGIN
{
	/* Print header */
	printf("%5s %12s %5s %-6s %s\n",
	    "FROM", "COMMAND", "SIG", "TO", "RESULT");
}

syscall::kill:entry
{
	/* Record target PID and signal */
	self->target = arg0;
	self->signal = arg1;
}

syscall::kill:return
{
	/* Print source, target, and result */
	printf("%5d %12s %5d %-6d %d\n",
	    pid, execname, self->signal, self->target, (int)arg0);

	/* Cleanup memory */
	self->target = 0;
	self->signal = 0;
}
