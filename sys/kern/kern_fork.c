/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_fork.c	8.6 (Berkeley) 4/8/94
 * $FreeBSD$
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/ktrace.h>
#include <sys/unistd.h>	
#include <sys/jail.h>	

#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

#include <sys/vmmeter.h>
#include <sys/user.h>

static MALLOC_DEFINE(M_ATFORK, "atfork", "atfork callback");

/*
 * These are the stuctures used to create a callout list for things to do
 * when forking a process
 */
struct forklist {
	forklist_fn function;
	TAILQ_ENTRY(forklist) next;
};

TAILQ_HEAD(forklist_head, forklist);
static struct forklist_head fork_list = TAILQ_HEAD_INITIALIZER(fork_list);

#ifndef _SYS_SYSPROTO_H_
struct fork_args {
	int     dummy;
};
#endif

int forksleep; /* Place for fork1() to sleep on. */

/* ARGSUSED */
int
fork(p, uap)
	struct proc *p;
	struct fork_args *uap;
{
	int error;
	struct proc *p2;

	error = fork1(p, RFFDG | RFPROC, &p2);
	if (error == 0) {
		p->p_retval[0] = p2->p_pid;
		p->p_retval[1] = 0;
	}
	return error;
}

/* ARGSUSED */
int
vfork(p, uap)
	struct proc *p;
	struct vfork_args *uap;
{
	int error;
	struct proc *p2;

	error = fork1(p, RFFDG | RFPROC | RFPPWAIT | RFMEM, &p2);
	if (error == 0) {
		p->p_retval[0] = p2->p_pid;
		p->p_retval[1] = 0;
	}
	return error;
}

int
rfork(p, uap)
	struct proc *p;
	struct rfork_args *uap;
{
	int error;
	struct proc *p2;

	error = fork1(p, uap->flags, &p2);
	if (error == 0) {
		p->p_retval[0] = p2 ? p2->p_pid : 0;
		p->p_retval[1] = 0;
	}
	return error;
}


int	nprocs = 1;		/* process 0 */
static int nextpid = 0;

/*
 * Random component to nextpid generation.  We mix in a random factor to make
 * it a little harder to predict.  We sanity check the modulus value to avoid
 * doing it in critical paths.  Don't let it be too small or we pointlessly
 * waste randomness entropy, and don't let it be impossibly large.  Using a
 * modulus that is too big causes a LOT more process table scans and slows
 * down fork processing as the pidchecked caching is defeated.
 */
static int randompid = 0;

static int
sysctl_kern_randompid(SYSCTL_HANDLER_ARGS)
{
		int error, pid;

		pid = randompid;
		error = sysctl_handle_int(oidp, &pid, 0, req);
		if (error || !req->newptr)
			return (error);
		if (pid < 0 || pid > PID_MAX - 100)	/* out of range */
			pid = PID_MAX - 100;
		else if (pid < 2)			/* NOP */
			pid = 0;
		else if (pid < 100)			/* Make it reasonable */
			pid = 100;
		randompid = pid;
		return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, randompid, CTLTYPE_INT|CTLFLAG_RW,
    0, 0, sysctl_kern_randompid, "I", "Random PID modulus");

int
fork1(p1, flags, procp)
	struct proc *p1;
	int flags;
	struct proc **procp;
{
	struct proc *p2, *pptr;
	uid_t uid;
	struct proc *newproc;
	int ok;
	static int pidchecked = 0;
	struct forklist *ep;

	if ((flags & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
		return (EINVAL);

	/*
	 * Here we don't create a new process, but we divorce
	 * certain parts of a process from itself.
	 */
	if ((flags & RFPROC) == 0) {

		vm_fork(p1, 0, flags);

		/*
		 * Close all file descriptors.
		 */
		if (flags & RFCFDG) {
			struct filedesc *fdtmp;
			fdtmp = fdinit(p1);
			fdfree(p1);
			p1->p_fd = fdtmp;
		}

		/*
		 * Unshare file descriptors (from parent.)
		 */
		if (flags & RFFDG) {
			if (p1->p_fd->fd_refcnt > 1) {
				struct filedesc *newfd;
				newfd = fdcopy(p1);
				fdfree(p1);
				p1->p_fd = newfd;
			}
		}
		*procp = NULL;
		return (0);
	}

	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create.  Don't allow
	 * a nonprivileged user to use the last process; don't let root
	 * exceed the limit. The variable nprocs is the current number of
	 * processes, maxproc is the limit.
	 */
	uid = p1->p_cred->p_ruid;
	if ((nprocs >= maxproc - 10 && uid != 0) || nprocs >= maxproc) {
		tsleep(&forksleep, PUSER, "fork", hz / 2);
		return (EAGAIN);
	}
	/*
	 * Increment the nprocs resource before blocking can occur.  There
	 * are hard-limits as to the number of processes that can run.
	 */
	nprocs++;

	/*
	 * Increment the count of procs running with this uid. Don't allow
	 * a nonprivileged user to exceed their current limit.
	 */
	ok = chgproccnt(p1->p_cred->p_uidinfo, 1,
		(uid != 0) ? p1->p_rlimit[RLIMIT_NPROC].rlim_cur : 0);
	if (!ok) {
		/*
		 * Back out the process count
		 */
		nprocs--;
		tsleep(&forksleep, PUSER, "fork", hz / 2);
		return (EAGAIN);
	}

	/* Allocate new proc. */
	newproc = zalloc(proc_zone);

	/*
	 * Setup linkage for kernel based threading
	 */
	if((flags & RFTHREAD) != 0) {
		newproc->p_peers = p1->p_peers;
		p1->p_peers = newproc;
		newproc->p_leader = p1->p_leader;
	} else {
		newproc->p_peers = 0;
		newproc->p_leader = newproc;
	}

	newproc->p_wakeup = 0;

	newproc->p_vmspace = NULL;

	/*
	 * Find an unused process ID.  We remember a range of unused IDs
	 * ready to use (from nextpid+1 through pidchecked-1).
	 */
	nextpid++;
	if (randompid)
		nextpid += arc4random() % randompid;
retry:
	/*
	 * If the process ID prototype has wrapped around,
	 * restart somewhat above 0, as the low-numbered procs
	 * tend to include daemons that don't exit.
	 */
	if (nextpid >= PID_MAX) {
		nextpid = nextpid % PID_MAX;
		if (nextpid < 100)
			nextpid += 100;
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
		p2 = LIST_FIRST(&allproc);
again:
		for (; p2 != 0; p2 = LIST_NEXT(p2, p_list)) {
			while (p2->p_pid == nextpid ||
			    p2->p_pgrp->pg_id == nextpid ||
			    p2->p_session->s_sid == nextpid) {
				nextpid++;
				if (nextpid >= pidchecked)
					goto retry;
			}
			if (p2->p_pid > nextpid && pidchecked > p2->p_pid)
				pidchecked = p2->p_pid;
			if (p2->p_pgrp->pg_id > nextpid &&
			    pidchecked > p2->p_pgrp->pg_id)
				pidchecked = p2->p_pgrp->pg_id;
			if (p2->p_session->s_sid > nextpid &&
			    pidchecked > p2->p_session->s_sid)
				pidchecked = p2->p_session->s_sid;
		}
		if (!doingzomb) {
			doingzomb = 1;
			p2 = LIST_FIRST(&zombproc);
			goto again;
		}
	}

	p2 = newproc;
	p2->p_stat = SIDL;			/* protect against others */
	p2->p_pid = nextpid;
	LIST_INSERT_HEAD(&allproc, p2, p_list);
	LIST_INSERT_HEAD(PIDHASH(p2->p_pid), p2, p_hash);

	/*
	 * Make a proc table entry for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */
	bzero(&p2->p_startzero,
	    (unsigned) ((caddr_t)&p2->p_endzero - (caddr_t)&p2->p_startzero));
	bcopy(&p1->p_startcopy, &p2->p_startcopy,
	    (unsigned) ((caddr_t)&p2->p_endcopy - (caddr_t)&p2->p_startcopy));

	p2->p_aioinfo = NULL;

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 * The p_stats and p_sigacts substructs are set in vm_fork.
	 */
	p2->p_flag = P_INMEM;
	if (p1->p_flag & P_PROFIL)
		startprofclock(p2);
	MALLOC(p2->p_cred, struct pcred *, sizeof(struct pcred),
	    M_SUBPROC, M_WAITOK);
	bcopy(p1->p_cred, p2->p_cred, sizeof(*p2->p_cred));
	p2->p_cred->p_refcnt = 1;
	crhold(p1->p_ucred);
	uihold(p1->p_cred->p_uidinfo);

	if (p2->p_prison) {
		p2->p_prison->pr_ref++;
		p2->p_flag |= P_JAILED;
	}

	if (p2->p_args)
		p2->p_args->ar_ref++;

	if (flags & RFSIGSHARE) {
		p2->p_procsig = p1->p_procsig;
		p2->p_procsig->ps_refcnt++;
		if (p1->p_sigacts == &p1->p_addr->u_sigacts) {
			struct sigacts *newsigacts;
			int s;

			/* Create the shared sigacts structure */
			MALLOC(newsigacts, struct sigacts *,
			    sizeof(struct sigacts), M_SUBPROC, M_WAITOK);
			s = splhigh();
			/*
			 * Set p_sigacts to the new shared structure.
			 * Note that this is updating p1->p_sigacts at the
			 * same time, since p_sigacts is just a pointer to
			 * the shared p_procsig->ps_sigacts.
			 */
			p2->p_sigacts  = newsigacts;
			bcopy(&p1->p_addr->u_sigacts, p2->p_sigacts,
			    sizeof(*p2->p_sigacts));
			*p2->p_sigacts = p1->p_addr->u_sigacts;
			splx(s);
		}
	} else {
		MALLOC(p2->p_procsig, struct procsig *, sizeof(struct procsig),
		    M_SUBPROC, M_WAITOK);
		bcopy(p1->p_procsig, p2->p_procsig, sizeof(*p2->p_procsig));
		p2->p_procsig->ps_refcnt = 1;
		p2->p_sigacts = NULL;	/* finished in vm_fork() */
	}
	if (flags & RFLINUXTHPN) 
	        p2->p_sigparent = SIGUSR1;
	else
	        p2->p_sigparent = SIGCHLD;

	/* bump references to the text vnode (for procfs) */
	p2->p_textvp = p1->p_textvp;
	if (p2->p_textvp)
		VREF(p2->p_textvp);

	if (flags & RFCFDG)
		p2->p_fd = fdinit(p1);
	else if (flags & RFFDG)
		p2->p_fd = fdcopy(p1);
	else
		p2->p_fd = fdshare(p1);

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

	/*
	 * Preserve some more flags in subprocess.  P_PROFIL has already
	 * been preserved.
	 */
	p2->p_flag |= p1->p_flag & (P_SUGID | P_ALTSTACK);
	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & P_CONTROLT)
		p2->p_flag |= P_CONTROLT;
	if (flags & RFPPWAIT)
		p2->p_flag |= P_PPWAIT;

	LIST_INSERT_AFTER(p1, p2, p_pglist);

	/*
	 * Attach the new process to its parent.
	 *
	 * If RFNOWAIT is set, the newly created process becomes a child
	 * of init.  This effectively disassociates the child from the
	 * parent.
	 */
	if (flags & RFNOWAIT)
		pptr = initproc;
	else
		pptr = p1;
	p2->p_pptr = pptr;
	LIST_INSERT_HEAD(&pptr->p_children, p2, p_sibling);
	LIST_INIT(&p2->p_children);

#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.  If not inherited,
	 * these were zeroed above but we still could have a trace race
	 * so make sure p2's p_tracep is NULL.
	 */
	if ((p1->p_traceflag & KTRFAC_INHERIT) && p2->p_tracep == NULL) {
		p2->p_traceflag = p1->p_traceflag;
		if ((p2->p_tracep = p1->p_tracep) != NULL)
			VREF(p2->p_tracep);
	}
#endif

	/*
	 * set priority of child to be that of parent
	 */
	p2->p_estcpu = p1->p_estcpu;

	/*
	 * This begins the section where we must prevent the parent
	 * from being swapped.
	 */
	PHOLD(p1);

	/*
	 * Finish creating the child process.  It will return via a different
	 * execution path later.  (ie: directly into user mode)
	 */
	vm_fork(p1, p2, flags);

	if (flags == (RFFDG | RFPROC)) {
		cnt.v_forks++;
		cnt.v_forkpages += p2->p_vmspace->vm_dsize + p2->p_vmspace->vm_ssize;
	} else if (flags == (RFFDG | RFPROC | RFPPWAIT | RFMEM)) {
		cnt.v_vforks++;
		cnt.v_vforkpages += p2->p_vmspace->vm_dsize + p2->p_vmspace->vm_ssize;
	} else if (p1 == &proc0) {
		cnt.v_kthreads++;
		cnt.v_kthreadpages += p2->p_vmspace->vm_dsize + p2->p_vmspace->vm_ssize;
	} else {
		cnt.v_rforks++;
		cnt.v_rforkpages += p2->p_vmspace->vm_dsize + p2->p_vmspace->vm_ssize;
	}

	/*
	 * Both processes are set up, now check if any loadable modules want
	 * to adjust anything.
	 *   What if they have an error? XXX
	 */
	TAILQ_FOREACH(ep, &fork_list, next) {
		(*ep->function)(p1, p2, flags);
	}

	/*
	 * Make child runnable and add to run queue.
	 */
	microtime(&(p2->p_stats->p_start));
	p2->p_acflag = AFORK;
	(void) splhigh();
	p2->p_stat = SRUN;
	setrunqueue(p2);
	(void) spl0();

	/*
	 * Now can be swapped.
	 */
	PRELE(p1);

	/*
	 * tell any interested parties about the new process
	 */
	KNOTE(&p1->p_klist, NOTE_FORK | p2->p_pid);

	/*
	 * Preserve synchronization semantics of vfork.  If waiting for
	 * child to exec or exit, set P_PPWAIT on child, and sleep on our
	 * proc (in case of exit).
	 */
	while (p2->p_flag & P_PPWAIT)
		tsleep(p1, PWAIT, "ppwait", 0);

	/*
	 * Return child proc pointer to parent.
	 */
	*procp = p2;
	return (0);
}

/*
 * The next two functionms are general routines to handle adding/deleting
 * items on the fork callout list.
 *
 * at_fork():
 * Take the arguments given and put them onto the fork callout list,
 * However first make sure that it's not already there.
 * Returns 0 on success or a standard error number.
 */

int
at_fork(function)
	forklist_fn function;
{
	struct forklist *ep;

#ifdef INVARIANTS
	/* let the programmer know if he's been stupid */
	if (rm_at_fork(function)) 
		printf("WARNING: fork callout entry (%p) already present\n",
		    function);
#endif
	ep = malloc(sizeof(*ep), M_ATFORK, M_NOWAIT);
	if (ep == NULL)
		return (ENOMEM);
	ep->function = function;
	TAILQ_INSERT_TAIL(&fork_list, ep, next);
	return (0);
}

/*
 * Scan the exit callout list for the given item and remove it..
 * Returns the number of items removed (0 or 1)
 */

int
rm_at_fork(function)
	forklist_fn function;
{
	struct forklist *ep;

	TAILQ_FOREACH(ep, &fork_list, next) {
		if (ep->function == function) {
			TAILQ_REMOVE(&fork_list, ep, next);
			free(ep, M_ATFORK);
			return(1);
		}
	}	
	return (0);
}
