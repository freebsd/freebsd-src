/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)kern_fork.c	7.29 (Berkeley) 5/15/91
 *	$Id: kern_fork.c,v 1.8 1994/05/04 08:26:54 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "filedesc.h"
#include "kernel.h"
#include "malloc.h"
#include "proc.h"
#include "resourcevar.h"
#include "vnode.h"
#include "file.h"
#include "acct.h"
#include "ktrace.h"
#include "vm/vm.h"

static int fork1(struct proc *, int, int *);

/* ARGSUSED */
int
fork(p, uap, retval)
	struct proc *p;
	void *uap;
	int retval[];
{

	return (fork1(p, 0, retval));
}

/* ARGSUSED */
int
vfork(p, uap, retval)
	struct proc *p;
	void *uap;
	int retval[];
{

	return (fork1(p, 1, retval));
}

int	nprocs = 1;		/* process 0 */

static int
fork1(p1, isvfork, retval)
	register struct proc *p1;
	int isvfork, retval[];
{
	register struct proc *p2;
	register int count, uid;
	static int nextpid, pidchecked = 0;

	count = 0;
	if ((uid = p1->p_ucred->cr_uid) != 0) {
		for (p2 = allproc; p2; p2 = p2->p_nxt)
			if (p2->p_ucred->cr_uid == uid)
				count++;
		for (p2 = zombproc; p2; p2 = p2->p_nxt)
			if (p2->p_ucred->cr_uid == uid)
				count++;
	}
	/*
	 * Although process entries are dynamically entries,
	 * we still keep a global limit on the maximum number
	 * we will create.  Don't allow a nonprivileged user
	 * to exceed its current limit or to bring us within one
	 * of the global limit; don't let root exceed the limit.
	 * nprocs is the current number of processes,
	 * maxproc is the limit.
	 */
	if (nprocs >= maxproc || uid == 0 && nprocs >= maxproc + 1) {
		tablefull("proc");
		return (EAGAIN);
	}
	if (count > p1->p_rlimit[RLIMIT_NPROC].rlim_cur)
		return (EAGAIN);

	/*
	 * Find an unused process ID.
	 * We remember a range of unused IDs ready to use
	 * (from nextpid+1 through pidchecked-1).
	 */
	nextpid++;
retry:
	/*
	 * If the process ID prototype has wrapped around,
	 * restart somewhat above 0, as the low-numbered procs
	 * tend to include daemons that don't exit.
	 */
	if (nextpid >= PID_MAX) {
		nextpid = 100;
		pidchecked = 0;
	}
	if (nextpid >= pidchecked) {
		int doingzomb = 0;

		pidchecked = PID_MAX;
		/*
		 * Scan the active and zombie procs to check whether this pid
		 * is in use.  Remember the lowest pid that's greater
		 * than nextpid, so we can avoid checking for a while.
		 */
		p2 = allproc;
again:
		for (; p2 != NULL; p2 = p2->p_nxt) {
			if (p2->p_pid == nextpid ||
			    p2->p_pgrp->pg_id == nextpid) {
				nextpid++;
				if (nextpid >= pidchecked)
					goto retry;
			}
			if (p2->p_pid > nextpid && pidchecked > p2->p_pid)
				pidchecked = p2->p_pid;
			if (p2->p_pgrp->pg_id > nextpid && 
			    pidchecked > p2->p_pgrp->pg_id)
				pidchecked = p2->p_pgrp->pg_id;
		}
		if (!doingzomb) {
			doingzomb = 1;
			p2 = zombproc;
			goto again;
		}
	}


	/*
	 * Allocate new proc.
	 * Link onto allproc (this should probably be delayed).
	 */
	MALLOC(p2, struct proc *, sizeof(struct proc), M_PROC, M_WAITOK);
	nprocs++;

	/* Initialize all fields to zero */
	bzero((struct proc *)p2, sizeof(struct proc));

	p2->p_nxt = allproc;
	p2->p_nxt->p_prev = &p2->p_nxt;		/* allproc is never NULL */
	p2->p_prev = &allproc;
	allproc = p2;

	/*
	 * Make a proc table entry for the new process.
	 * Copy the section that is copied directly from the parent.
	 */
	bcopy(&p1->p_startcopy, &p2->p_startcopy,
	    (unsigned) ((caddr_t)&p2->p_endcopy - (caddr_t)&p2->p_startcopy));

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 * The p_stats and p_sigacts substructs are set in vm_fork.
	 */
	MALLOC(p2->p_cred, struct pcred *, sizeof(struct pcred),
	    M_SUBPROC, M_WAITOK);
	bcopy(p1->p_cred, p2->p_cred, sizeof(*p2->p_cred));
	p2->p_cred->p_refcnt = 1;
	crhold(p1->p_ucred);

	p2->p_fd = fdcopy(p1);
	/*
	 * If p_limit is still copy-on-write, bump refcnt,
	 * otherwise get a copy that won't be modified.
	 * (If PL_SHAREMOD is clear, the structure is shared
	 * copy-on-write.)
	 */
	if (p1->p_limit->p_lflags & PL_SHAREMOD)
		p2->p_limit = limcopy(p1->p_limit);
	else {
		p2->p_limit = p1->p_limit;
		p2->p_limit->p_refcnt++;
	}

	p2->p_flag = SLOAD | (p1->p_flag & SHPUX);
	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & SCTTY)
		p2->p_flag |= SCTTY;
	if (isvfork)
		p2->p_flag |= SPPWAIT;
	p2->p_stat = SIDL;
	p2->p_pid = nextpid;
	{
	struct proc **hash = &pidhash[PIDHASH(p2->p_pid)];

	p2->p_hash = *hash;
	*hash = p2;
	}
	p2->p_pgrpnxt = p1->p_pgrpnxt;
	p1->p_pgrpnxt = p2;
	p2->p_pptr = p1;
	p2->p_osptr = p1->p_cptr;
	if (p1->p_cptr)
		p1->p_cptr->p_ysptr = p2;
	p1->p_cptr = p2;
#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.
	 * If not inherited, these were zeroed above.
	 */
	if (p1->p_traceflag&KTRFAC_INHERIT) {
		p2->p_traceflag = p1->p_traceflag;
		if ((p2->p_tracep = p1->p_tracep) != NULL)
			VREF(p2->p_tracep);
	}
#endif

#if defined(tahoe)
	p2->p_vmspace->p_ckey = p1->p_vmspace->p_ckey; /* XXX move this */
#endif

	/*
	 * set priority of child to be that of parent
	 */
	p2->p_cpu = p1->p_cpu;

	/*
	 * This begins the section where we must prevent the parent
	 * from being swapped.
	 */
	p1->p_flag |= SKEEP;
	/*
	 * Set return values for child before vm_fork,
	 * so they can be copied to child stack.
	 * We return parent pid, and mark as child in retval[1].
	 * NOTE: the kernel stack may be at a different location in the child
	 * process, and thus addresses of automatic variables (including retval)
	 * may be invalid after vm_fork returns in the child process.
	 */
	retval[0] = p1->p_pid;
	retval[1] = 1;
	if (vm_fork(p1, p2, isvfork)) {
		/*
		 * Child process.  Set start time and get to work.
		 */
		(void) splclock();
		p2->p_stats->p_start = time;
		(void) spl0();
		p2->p_acflag = AFORK;
/*
		vm_map_init_pmap(&p2->p_vmspace->vm_map);
*/
		return (0);
	}

	/*
	 * Make child runnable and add to run queue.
	 */
	(void) splhigh();
	p2->p_stat = SRUN;
	setrq(p2);
	(void) spl0();

	/*
	 * Now can be swapped.
	 */
	p1->p_flag &= ~SKEEP;

	/*
	 * Preserve synchronization semantics of vfork.
	 * If waiting for child to exec or exit, set SPPWAIT
	 * on child, and sleep on our proc (in case of exit).
	 */
	if (isvfork)
		while (p2->p_flag & SPPWAIT)
			tsleep((caddr_t)p1, PWAIT, "ppwait", 0);

	/*
	 * Return child pid to parent process,
	 * marking us as parent via retval[1].
	 */
	retval[0] = p2->p_pid;
	retval[1] = 0;
	return (0);
}
