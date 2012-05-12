#!/usr/sbin/dtrace -Cs
/*
 * crash.d - Crashed Application info.
 *           Written in DTrace (Solaris 10 3/05).
 *
 * $Id: crash.d 3 2007-08-01 10:50:08Z brendan $
 *
 * When applications crash via a SIGSEGV or SIGBUS, a report of the
 * process state is printed out.
 *
 * USAGE:       crash.d
 *
 * FIELDS:
 *              Type		Signal type
 *              Program		Execname of process
 *              Agrs		Argument listing of process
 *              PID		Process ID
 *              TID		Thread ID
 *              LWPs		Number of Light Weight Processes
 *              PPID		Parent Process ID
 *              UID		User ID
 *              GID		Group ID
 *              TaskID		Task ID
 *              ProjID		Project ID
 *              PoolID		Pool ID
 *              ZoneID		Zone ID
 *              zone		Zone name
 *              CWD		Current working directory
 *              errno		Error number of last syscall
 *
 * SEE ALSO: mdb, pstack, coreadm
 *           app_crash.d - Greg Nakhimovsky & Morgan Herrington
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
 * 29-May-2005  Brendan Gregg   Created this.
 * 24-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet
#pragma D option destructive

dtrace:::BEGIN
{
	printf("Waiting for crashing applications...\n");
}

/*
 * Print Report Header
 */
proc:::signal-send
/(args[2] == SIGBUS || args[2] == SIGSEGV) && pid == args[1]->pr_pid/
{
	stop();
	self->elapsed = timestamp - curthread->t_procp->p_mstart;
	self->crash = 1;

	printf("\n-----------------------------------------------------\n");
	printf("CRASH DETECTED at %Y\n", walltimestamp);
	printf("-----------------------------------------------------\n");
	printf("Type:    %s\n", args[2] == SIGBUS ? "SIGBUS" : "SIGSEGV");
	printf("Program: %s\n", execname);
	printf("Args:    %S\n", curpsinfo->pr_psargs);
	printf("PID:     %d\n", pid);
	printf("TID:     %d\n", tid);
	printf("LWPs:    %d\n", curthread->t_procp->p_lwpcnt);
	printf("PPID:    %d\n", ppid);
	printf("UID:     %d\n", uid);
	printf("GID:     %d\n", gid);
	printf("TaskID:  %d\n", curpsinfo->pr_taskid);
	printf("ProjID:  %d\n", curpsinfo->pr_projid);
	printf("PoolID:  %d\n", curpsinfo->pr_poolid);
	printf("ZoneID:  %d\n", curpsinfo->pr_zoneid);
	printf("zone:    %s\n", zonename);
	printf("CWD:     %s\n", cwd);
	printf("errno:   %d\n", errno);

	printf("\nUser Stack Backtrace,");
	ustack();

	printf("\nKernel Stack Backtrace,");
	stack();
}

/*
 * Print Java Details
 */
proc:::signal-send
/self->crash && execname == "java"/
{
	printf("\nJava Stack Backtrace,");
	jstack();
}

/*
 * Print Ancestors
 */
proc:::signal-send
/self->crash/
{
	printf("\nAnsestors,\n");
	self->level = 1;
	self->procp = curthread->t_procp;
	self->ptr = self->procp;
}

/* ancestory un-rolled loop, reverse order, 6 deep */
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}
proc:::signal-send /self->crash && self->ptr != 0/
{
	printf("%*s %d %S\n", self->level += 2, "",
	    self->ptr->p_pidp->pid_id, self->ptr->p_user.u_psargs);
	self->ptr = self->ptr->p_parent;
}

/*
 * Print Report Footer
 */
proc:::signal-send
/self->crash/
{

	printf("\nTimes,\n");
	printf("    User:    %d ticks\n", self->procp->p_utime);
	printf("    Sys:     %d ticks\n", self->procp->p_stime);
	printf("    Elapsed: %d ms\n", self->elapsed/1000000);

	printf("\nSizes,\n");
	printf("    Heap:   %d bytes\n", self->procp->p_brksize);
	printf("    Stack:  %d bytes\n", self->procp->p_stksize);

	self->ptr = 0;
	self->procp = 0;
	self->crash = 0;
	self->level = 0;
	self->elapsed = 0;
	system("/usr/bin/prun %d", pid);
}
