/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdtrace.h"
#include "opt_ktrace.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/eventhandler.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/unistd.h>	
#include <sys/sdt.h>
#include <sys/sx.h>
#include <sys/signalvar.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
dtrace_fork_func_t	dtrace_fasttrap_fork;
#endif

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE(proc, kernel, , create, create);
SDT_PROBE_ARGTYPE(proc, kernel, , create, 0, "struct proc *");
SDT_PROBE_ARGTYPE(proc, kernel, , create, 1, "struct proc *");
SDT_PROBE_ARGTYPE(proc, kernel, , create, 2, "int");

#ifndef _SYS_SYSPROTO_H_
struct fork_args {
	int     dummy;
};
#endif

/* ARGSUSED */
int
fork(struct thread *td, struct fork_args *uap)
{
	int error;
	struct proc *p2;

	error = fork1(td, RFFDG | RFPROC, 0, &p2);
	if (error == 0) {
		td->td_retval[0] = p2->p_pid;
		td->td_retval[1] = 0;
	}
	return (error);
}

/* ARGSUSED */
int
vfork(td, uap)
	struct thread *td;
	struct vfork_args *uap;
{
	int error, flags;
	struct proc *p2;

#ifdef XEN
	flags = RFFDG | RFPROC; /* validate that this is still an issue */
#else
	flags = RFFDG | RFPROC | RFPPWAIT | RFMEM;
#endif		
	error = fork1(td, flags, 0, &p2);
	if (error == 0) {
		td->td_retval[0] = p2->p_pid;
		td->td_retval[1] = 0;
	}
	return (error);
}

int
rfork(struct thread *td, struct rfork_args *uap)
{
	struct proc *p2;
	int error;

	/* Don't allow kernel-only flags. */
	if ((uap->flags & RFKERNELONLY) != 0)
		return (EINVAL);

	AUDIT_ARG_FFLAGS(uap->flags);
	error = fork1(td, uap->flags, 0, &p2);
	if (error == 0) {
		td->td_retval[0] = p2 ? p2->p_pid : 0;
		td->td_retval[1] = 0;
	}
	return (error);
}

int	nprocs = 1;		/* process 0 */
int	lastpid = 0;
SYSCTL_INT(_kern, OID_AUTO, lastpid, CTLFLAG_RD, &lastpid, 0, 
    "Last used PID");

/*
 * Random component to lastpid generation.  We mix in a random factor to make
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

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error != 0)
		return(error);
	sx_xlock(&allproc_lock);
	pid = randompid;
	error = sysctl_handle_int(oidp, &pid, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (pid < 0 || pid > PID_MAX - 100)	/* out of range */
			pid = PID_MAX - 100;
		else if (pid < 2)			/* NOP */
			pid = 0;
		else if (pid < 100)			/* Make it reasonable */
			pid = 100;
		randompid = pid;
	}
	sx_xunlock(&allproc_lock);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, randompid, CTLTYPE_INT|CTLFLAG_RW,
    0, 0, sysctl_kern_randompid, "I", "Random PID modulus");

static int
fork_norfproc(struct thread *td, int flags, struct proc **procp)
{
	int error;
	struct proc *p1;

	KASSERT((flags & RFPROC) == 0,
	    ("fork_norfproc called with RFPROC set"));
	p1 = td->td_proc;
	*procp = NULL;

	if (((p1->p_flag & (P_HADTHREADS|P_SYSTEM)) == P_HADTHREADS) &&
	    (flags & (RFCFDG | RFFDG))) {
		PROC_LOCK(p1);
		if (thread_single(SINGLE_BOUNDARY)) {
			PROC_UNLOCK(p1);
			return (ERESTART);
		}
		PROC_UNLOCK(p1);
	}

	error = vm_forkproc(td, NULL, NULL, NULL, flags);
	if (error)
		goto fail;

	/*
	 * Close all file descriptors.
	 */
	if (flags & RFCFDG) {
		struct filedesc *fdtmp;
		fdtmp = fdinit(td->td_proc->p_fd);
		fdfree(td);
		p1->p_fd = fdtmp;
	}

	/*
	 * Unshare file descriptors (from parent).
	 */
	if (flags & RFFDG) 
		fdunshare(p1, td);

fail:
	if (((p1->p_flag & (P_HADTHREADS|P_SYSTEM)) == P_HADTHREADS) &&
	    (flags & (RFCFDG | RFFDG))) {
		PROC_LOCK(p1);
		thread_single_end();
		PROC_UNLOCK(p1);
	}
	return (error);
}

int
fork1(struct thread *td, int flags, int pages, struct proc **procp)
{
	struct proc *p1, *p2, *pptr;
	struct proc *newproc;
	int ok, trypid;
	static int curfail, pidchecked = 0;
	static struct timeval lastfail;
	struct filedesc *fd;
	struct filedesc_to_leader *fdtol;
	struct thread *td2;
	struct sigacts *newsigacts;
	struct vmspace *vm2;
	vm_ooffset_t mem_charged;
	int error;

	/* Can't copy and clear. */
	if ((flags & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
		return (EINVAL);

	p1 = td->td_proc;

	/*
	 * Here we don't create a new process, but we divorce
	 * certain parts of a process from itself.
	 */
	if ((flags & RFPROC) == 0)
		return (fork_norfproc(td, flags, procp));

	/*
	 * XXX
	 * We did have single-threading code here
	 * however it proved un-needed and caused problems
	 */

	mem_charged = 0;
	vm2 = NULL;
	if (pages == 0)
		pages = KSTACK_PAGES;
	/* Allocate new proc. */
	newproc = uma_zalloc(proc_zone, M_WAITOK);
	td2 = FIRST_THREAD_IN_PROC(newproc);
	if (td2 == NULL) {
		td2 = thread_alloc(pages);
		if (td2 == NULL) {
			error = ENOMEM;
			goto fail1;
		}
		proc_linkup(newproc, td2);
	} else {
		if (td2->td_kstack == 0 || td2->td_kstack_pages != pages) {
			if (td2->td_kstack != 0)
				vm_thread_dispose(td2);
			if (!thread_alloc_stack(td2, pages)) {
				error = ENOMEM;
				goto fail1;
			}
		}
	}

	if ((flags & RFMEM) == 0) {
		vm2 = vmspace_fork(p1->p_vmspace, &mem_charged);
		if (vm2 == NULL) {
			error = ENOMEM;
			goto fail1;
		}
		if (!swap_reserve(mem_charged)) {
			/*
			 * The swap reservation failed. The accounting
			 * from the entries of the copied vm2 will be
			 * substracted in vmspace_free(), so force the
			 * reservation there.
			 */
			swap_reserve_force(mem_charged);
			error = ENOMEM;
			goto fail1;
		}
	} else
		vm2 = NULL;
#ifdef MAC
	mac_proc_init(newproc);
#endif
	knlist_init_mtx(&newproc->p_klist, &newproc->p_mtx);
	STAILQ_INIT(&newproc->p_ktr);

	/* We have to lock the process tree while we look for a pid. */
	sx_slock(&proctree_lock);

	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create.  Don't allow
	 * a nonprivileged user to use the last ten processes; don't let root
	 * exceed the limit. The variable nprocs is the current number of
	 * processes, maxproc is the limit.
	 */
	sx_xlock(&allproc_lock);
	if ((nprocs >= maxproc - 10 && priv_check_cred(td->td_ucred,
	    PRIV_MAXPROC, 0) != 0) || nprocs >= maxproc) {
		error = EAGAIN;
		goto fail;
	}

	/*
	 * Increment the count of procs running with this uid. Don't allow
	 * a nonprivileged user to exceed their current limit.
	 *
	 * XXXRW: Can we avoid privilege here if it's not needed?
	 */
	error = priv_check_cred(td->td_ucred, PRIV_PROC_LIMIT, 0);
	if (error == 0)
		ok = chgproccnt(td->td_ucred->cr_ruidinfo, 1, 0);
	else {
		PROC_LOCK(p1);
		ok = chgproccnt(td->td_ucred->cr_ruidinfo, 1,
		    lim_cur(p1, RLIMIT_NPROC));
		PROC_UNLOCK(p1);
	}
	if (!ok) {
		error = EAGAIN;
		goto fail;
	}

	/*
	 * Increment the nprocs resource before blocking can occur.  There
	 * are hard-limits as to the number of processes that can run.
	 */
	nprocs++;

	/*
	 * Find an unused process ID.  We remember a range of unused IDs
	 * ready to use (from lastpid+1 through pidchecked-1).
	 *
	 * If RFHIGHPID is set (used during system boot), do not allocate
	 * low-numbered pids.
	 */
	trypid = lastpid + 1;
	if (flags & RFHIGHPID) {
		if (trypid < 10)
			trypid = 10;
	} else {
		if (randompid)
			trypid += arc4random() % randompid;
	}
retry:
	/*
	 * If the process ID prototype has wrapped around,
	 * restart somewhat above 0, as the low-numbered procs
	 * tend to include daemons that don't exit.
	 */
	if (trypid >= PID_MAX) {
		trypid = trypid % PID_MAX;
		if (trypid < 100)
			trypid += 100;
		pidchecked = 0;
	}
	if (trypid >= pidchecked) {
		int doingzomb = 0;

		pidchecked = PID_MAX;
		/*
		 * Scan the active and zombie procs to check whether this pid
		 * is in use.  Remember the lowest pid that's greater
		 * than trypid, so we can avoid checking for a while.
		 */
		p2 = LIST_FIRST(&allproc);
again:
		for (; p2 != NULL; p2 = LIST_NEXT(p2, p_list)) {
			while (p2->p_pid == trypid ||
			    (p2->p_pgrp != NULL &&
			    (p2->p_pgrp->pg_id == trypid ||
			    (p2->p_session != NULL &&
			    p2->p_session->s_sid == trypid)))) {
				trypid++;
				if (trypid >= pidchecked)
					goto retry;
			}
			if (p2->p_pid > trypid && pidchecked > p2->p_pid)
				pidchecked = p2->p_pid;
			if (p2->p_pgrp != NULL) {
				if (p2->p_pgrp->pg_id > trypid &&
				    pidchecked > p2->p_pgrp->pg_id)
					pidchecked = p2->p_pgrp->pg_id;
				if (p2->p_session != NULL &&
				    p2->p_session->s_sid > trypid &&
				    pidchecked > p2->p_session->s_sid)
					pidchecked = p2->p_session->s_sid;
			}
		}
		if (!doingzomb) {
			doingzomb = 1;
			p2 = LIST_FIRST(&zombproc);
			goto again;
		}
	}
	sx_sunlock(&proctree_lock);

	/*
	 * RFHIGHPID does not mess with the lastpid counter during boot.
	 */
	if (flags & RFHIGHPID)
		pidchecked = 0;
	else
		lastpid = trypid;

	p2 = newproc;
	p2->p_state = PRS_NEW;		/* protect against others */
	p2->p_pid = trypid;
	/*
	 * Allow the scheduler to initialize the child.
	 */
	thread_lock(td);
	sched_fork(td, td2);
	thread_unlock(td);
	AUDIT_ARG_PID(p2->p_pid);
	LIST_INSERT_HEAD(&allproc, p2, p_list);
	LIST_INSERT_HEAD(PIDHASH(p2->p_pid), p2, p_hash);
	tidhash_add(td2);
	PROC_LOCK(p2);
	PROC_LOCK(p1);

	sx_xunlock(&allproc_lock);

	bcopy(&p1->p_startcopy, &p2->p_startcopy,
	    __rangeof(struct proc, p_startcopy, p_endcopy));
	pargs_hold(p2->p_args);
	PROC_UNLOCK(p1);

	bzero(&p2->p_startzero,
	    __rangeof(struct proc, p_startzero, p_endzero));

	p2->p_ucred = crhold(td->td_ucred);

	/* Tell the prison that we exist. */
	prison_proc_hold(p2->p_ucred->cr_prison);

	PROC_UNLOCK(p2);

	/*
	 * Malloc things while we don't hold any locks.
	 */
	if (flags & RFSIGSHARE)
		newsigacts = NULL;
	else
		newsigacts = sigacts_alloc();

	/*
	 * Copy filedesc.
	 */
	if (flags & RFCFDG) {
		fd = fdinit(p1->p_fd);
		fdtol = NULL;
	} else if (flags & RFFDG) {
		fd = fdcopy(p1->p_fd);
		fdtol = NULL;
	} else {
		fd = fdshare(p1->p_fd);
		if (p1->p_fdtol == NULL)
			p1->p_fdtol =
				filedesc_to_leader_alloc(NULL,
							 NULL,
							 p1->p_leader);
		if ((flags & RFTHREAD) != 0) {
			/*
			 * Shared file descriptor table and
			 * shared process leaders.
			 */
			fdtol = p1->p_fdtol;
			FILEDESC_XLOCK(p1->p_fd);
			fdtol->fdl_refcount++;
			FILEDESC_XUNLOCK(p1->p_fd);
		} else {
			/* 
			 * Shared file descriptor table, and
			 * different process leaders 
			 */
			fdtol = filedesc_to_leader_alloc(p1->p_fdtol,
							 p1->p_fd,
							 p2);
		}
	}
	/*
	 * Make a proc table entry for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */

	PROC_LOCK(p2);
	PROC_LOCK(p1);

	bzero(&td2->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));

	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));

	bcopy(&p2->p_comm, &td2->td_name, sizeof(td2->td_name));
	td2->td_sigstk = td->td_sigstk;
	td2->td_sigmask = td->td_sigmask;
	td2->td_flags = TDF_INMEM;

#ifdef VIMAGE
	td2->td_vnet = NULL;
	td2->td_vnet_lpush = NULL;
#endif

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 */
	p2->p_flag = P_INMEM;
	p2->p_swtick = ticks;
	if (p1->p_flag & P_PROFIL)
		startprofclock(p2);
	td2->td_ucred = crhold(p2->p_ucred);

	if (flags & RFSIGSHARE) {
		p2->p_sigacts = sigacts_hold(p1->p_sigacts);
	} else {
		sigacts_copy(newsigacts, p1->p_sigacts);
		p2->p_sigacts = newsigacts;
	}
	if (flags & RFLINUXTHPN) 
	        p2->p_sigparent = SIGUSR1;
	else
	        p2->p_sigparent = SIGCHLD;

	p2->p_textvp = p1->p_textvp;
	p2->p_fd = fd;
	p2->p_fdtol = fdtol;

	/*
	 * p_limit is copy-on-write.  Bump its refcount.
	 */
	lim_fork(p1, p2);

	pstats_fork(p1->p_stats, p2->p_stats);

	PROC_UNLOCK(p1);
	PROC_UNLOCK(p2);

	/* Bump references to the text vnode (for procfs) */
	if (p2->p_textvp)
		vref(p2->p_textvp);

	/*
	 * Set up linkage for kernel based threading.
	 */
	if ((flags & RFTHREAD) != 0) {
		mtx_lock(&ppeers_lock);
		p2->p_peers = p1->p_peers;
		p1->p_peers = p2;
		p2->p_leader = p1->p_leader;
		mtx_unlock(&ppeers_lock);
		PROC_LOCK(p1->p_leader);
		if ((p1->p_leader->p_flag & P_WEXIT) != 0) {
			PROC_UNLOCK(p1->p_leader);
			/*
			 * The task leader is exiting, so process p1 is
			 * going to be killed shortly.  Since p1 obviously
			 * isn't dead yet, we know that the leader is either
			 * sending SIGKILL's to all the processes in this
			 * task or is sleeping waiting for all the peers to
			 * exit.  We let p1 complete the fork, but we need
			 * to go ahead and kill the new process p2 since
			 * the task leader may not get a chance to send
			 * SIGKILL to it.  We leave it on the list so that
			 * the task leader will wait for this new process
			 * to commit suicide.
			 */
			PROC_LOCK(p2);
			psignal(p2, SIGKILL);
			PROC_UNLOCK(p2);
		} else
			PROC_UNLOCK(p1->p_leader);
	} else {
		p2->p_peers = NULL;
		p2->p_leader = p2;
	}

	sx_xlock(&proctree_lock);
	PGRP_LOCK(p1->p_pgrp);
	PROC_LOCK(p2);
	PROC_LOCK(p1);

	/*
	 * Preserve some more flags in subprocess.  P_PROFIL has already
	 * been preserved.
	 */
	p2->p_flag |= p1->p_flag & P_SUGID;
	td2->td_pflags |= td->td_pflags & TDP_ALTSTACK;
	SESS_LOCK(p1->p_session);
	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & P_CONTROLT)
		p2->p_flag |= P_CONTROLT;
	SESS_UNLOCK(p1->p_session);
	if (flags & RFPPWAIT)
		p2->p_flag |= P_PPWAIT;

	p2->p_pgrp = p1->p_pgrp;
	LIST_INSERT_AFTER(p1, p2, p_pglist);
	PGRP_UNLOCK(p1->p_pgrp);
	LIST_INIT(&p2->p_children);

	callout_init(&p2->p_itcallout, CALLOUT_MPSAFE);

#ifdef KTRACE
	ktrprocfork(p1, p2);
#endif

	/*
	 * If PF_FORK is set, the child process inherits the
	 * procfs ioctl flags from its parent.
	 */
	if (p1->p_pfsflags & PF_FORK) {
		p2->p_stops = p1->p_stops;
		p2->p_pfsflags = p1->p_pfsflags;
	}

	/*
	 * This begins the section where we must prevent the parent
	 * from being swapped.
	 */
	_PHOLD(p1);
	PROC_UNLOCK(p1);

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
	sx_xunlock(&proctree_lock);

	/* Inform accounting that we have forked. */
	p2->p_acflag = AFORK;
	PROC_UNLOCK(p2);

	/*
	 * Finish creating the child process.  It will return via a different
	 * execution path later.  (ie: directly into user mode)
	 */
	vm_forkproc(td, p2, td2, vm2, flags);

	if (flags == (RFFDG | RFPROC)) {
		PCPU_INC(cnt.v_forks);
		PCPU_ADD(cnt.v_forkpages, p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize);
	} else if (flags == (RFFDG | RFPROC | RFPPWAIT | RFMEM)) {
		PCPU_INC(cnt.v_vforks);
		PCPU_ADD(cnt.v_vforkpages, p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize);
	} else if (p1 == &proc0) {
		PCPU_INC(cnt.v_kthreads);
		PCPU_ADD(cnt.v_kthreadpages, p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize);
	} else {
		PCPU_INC(cnt.v_rforks);
		PCPU_ADD(cnt.v_rforkpages, p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize);
	}

	/*
	 * Both processes are set up, now check if any loadable modules want
	 * to adjust anything.
	 *   What if they have an error? XXX
	 */
	EVENTHANDLER_INVOKE(process_fork, p1, p2, flags);

	/*
	 * Set the child start time and mark the process as being complete.
	 */
	microuptime(&p2->p_stats->p_start);
	PROC_SLOCK(p2);
	p2->p_state = PRS_NORMAL;
	PROC_SUNLOCK(p2);
#ifdef KDTRACE_HOOKS
	/*
	 * Tell the DTrace fasttrap provider about the new process
	 * if it has registered an interest. We have to do this only after
	 * p_state is PRS_NORMAL since the fasttrap module will use pfind()
	 * later on.
	 */
	if (dtrace_fasttrap_fork) {
		PROC_LOCK(p1);
		PROC_LOCK(p2);
		dtrace_fasttrap_fork(p1, p2);
		PROC_UNLOCK(p2);
		PROC_UNLOCK(p1);
	}
#endif

	/*
	 * If RFSTOPPED not requested, make child runnable and add to
	 * run queue.
	 */
	if ((flags & RFSTOPPED) == 0) {
		thread_lock(td2);
		TD_SET_CAN_RUN(td2);
		sched_add(td2, SRQ_BORING);
		thread_unlock(td2);
	}

	/*
	 * Now can be swapped.
	 */
	PROC_LOCK(p1);
	_PRELE(p1);
	PROC_UNLOCK(p1);

	/*
	 * Tell any interested parties about the new process.
	 */
	knote_fork(&p1->p_klist, p2->p_pid);
	SDT_PROBE(proc, kernel, , create, p2, p1, flags, 0, 0);

	/*
	 * Preserve synchronization semantics of vfork.  If waiting for
	 * child to exec or exit, set P_PPWAIT on child, and sleep on our
	 * proc (in case of exit).
	 */
	PROC_LOCK(p2);
	while (p2->p_flag & P_PPWAIT)
		cv_wait(&p2->p_pwait, &p2->p_mtx);
	PROC_UNLOCK(p2);

	/*
	 * Return child proc pointer to parent.
	 */
	*procp = p2;
	return (0);
fail:
	sx_sunlock(&proctree_lock);
	if (ppsratecheck(&lastfail, &curfail, 1))
		printf("maxproc limit exceeded by uid %i, please see tuning(7) and login.conf(5).\n",
		    td->td_ucred->cr_ruid);
	sx_xunlock(&allproc_lock);
#ifdef MAC
	mac_proc_destroy(newproc);
#endif
fail1:
	if (vm2 != NULL)
		vmspace_free(vm2);
	uma_zfree(proc_zone, newproc);
	pause("fork", hz / 2);
	return (error);
}

/*
 * Handle the return of a child process from fork1().  This function
 * is called from the MD fork_trampoline() entry point.
 */
void
fork_exit(void (*callout)(void *, struct trapframe *), void *arg,
    struct trapframe *frame)
{
	struct proc *p;
	struct thread *td;
	struct thread *dtd;

	td = curthread;
	p = td->td_proc;
	KASSERT(p->p_state == PRS_NORMAL, ("executing process is still new"));

	CTR4(KTR_PROC, "fork_exit: new thread %p (td_sched %p, pid %d, %s)",
		td, td->td_sched, p->p_pid, td->td_name);

	sched_fork_exit(td);
	/*
	* Processes normally resume in mi_switch() after being
	* cpu_switch()'ed to, but when children start up they arrive here
	* instead, so we must do much the same things as mi_switch() would.
	*/
	if ((dtd = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(dtd);
	}
	thread_unlock(td);

	/*
	 * cpu_set_fork_handler intercepts this function call to
	 * have this call a non-return function to stay in kernel mode.
	 * initproc has its own fork handler, but it does return.
	 */
	KASSERT(callout != NULL, ("NULL callout in fork_exit"));
	callout(arg, frame);

	/*
	 * Check if a kernel thread misbehaved and returned from its main
	 * function.
	 */
	if (p->p_flag & P_KTHREAD) {
		printf("Kernel thread \"%s\" (pid %d) exited prematurely.\n",
		    td->td_name, p->p_pid);
		kproc_exit(0);
	}
	mtx_assert(&Giant, MA_NOTOWNED);

	EVENTHANDLER_INVOKE(schedtail, p);
}

/*
 * Simplified back end of syscall(), used when returning from fork()
 * directly into user mode.  Giant is not held on entry, and must not
 * be held on return.  This function is passed in to fork_exit() as the
 * first parameter and is called when returning to a new userland process.
 */
void
fork_return(struct thread *td, struct trapframe *frame)
{

	userret(td, frame);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(SYS_fork, 0, 0);
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
}
