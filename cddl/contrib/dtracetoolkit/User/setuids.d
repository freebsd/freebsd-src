#!/usr/sbin/dtrace -s
/*
 * setuids.d - snoop setuid calls. This can examine user logins.
 *             Written in DTrace (Solaris 10 3/05).
 *
 * $Id: setuids.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	setuids.d
 *
 * FIELDS:
 *		UID	user ID (from)
 *		SUID	set user ID (to)
 *		PPID	parent process ID
 *		PID	process ID
 *		PCMD	parent command
 *		CMD	command (full arguments)
 *
 * SEE ALSO: BSM auditing
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
 * 09-May-2004	Brendan Gregg	Created this.
 * 08-May-2005	   "      " 	Used modern variable builtins.
 * 28-Jul-2005	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("%5s %5s %5s %5s %-12s %s\n",
	    "UID", "SUID", "PPID", "PID", "PCMD", "CMD");
}

/*
 * Save values
 */
syscall::setuid:entry
{
	self->uid = uid;
	self->suid = arg0;
	self->ok = 1;
}

/*
 * Print output on success
 */
syscall::setuid:return
/arg0 == 0 && self->ok/
{
	printf("%5d %5d %5d %5d %-12s %S\n",
	    self->uid, self->suid, ppid, pid,
	    curthread->t_procp->p_parent->p_user.u_comm,
	    curpsinfo->pr_psargs);
}

/*
 * Cleanup
 */
syscall::setuid:return
{
	self->uid = 0;
	self->suid = 0;
	self->ok = 0;
}
