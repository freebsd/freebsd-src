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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/eventhandler.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/mac.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/unistd.h>	
#include <sys/jail.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <sys/user.h>
#include <machine/critical.h>

#ifndef _SYS_SYSPROTO_H_
struct fork_args {
	int     dummy;
};
#endif

static int forksleep; /* Place for fork1() to sleep on. */

/*
 * MPSAFE
 */
/* ARGSUSED */
int
fork(td, uap)
	struct thread *td;
	struct fork_args *uap;
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

/*
 * MPSAFE
 */
/* ARGSUSED */
int
vfork(td, uap)
	struct thread *td;
	struct vfork_args *uap;
{
	int error;
	struct proc *p2;

	error = fork1(td, RFFDG | RFPROC | RFPPWAIT | RFMEM, 0, &p2);
	if (error == 0) {
		td->td_retval[0] = p2->p_pid;
		td->td_retval[1] = 0;
	}
	return (error);
}

/*
 * MPSAFE
 */
int
rfork(td, uap)
	struct thread *td;
	struct rfork_args *uap;
{
	int error;
	struct proc *p2;

	/* Don't allow kernel only flags. */
	if ((uap->flags & RFKERNELONLY) != 0)
		return (EINVAL);
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

	sysctl_wire_old_buffer(req, sizeof(int));
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

int
fork1(td, flags, pages, procp)
	struct thread *td;
	int flags;
	int pages;
	struct proc **procp;
{
	struct proc *p1, *p2, *pptr;
	uid_t uid;
	struct proc *newproc;
	int ok, trypid;
	static int curfail, pidchecked = 0;
	static struct timeval lastfail;
	struct filedesc *fd;
	struct filedesc_to_leader *fdtol;
	struct thread *td2;
	struct kse *ke2;
	struct ksegrp *kg2;
	struct sigacts *newsigacts;
	int error;

	/* Can't copy and clear. */
	if ((flags & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
		return (EINVAL);

	p1 = td->td_proc;
	mtx_lock(&Giant);

	/*
	 * Here we don't create a new process, but we divorce
	 * certain parts of a process from itself.
	 */
	if ((flags & RFPROC) == 0) {
		vm_forkproc(td, NULL, NULL, flags);

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
		 * Unshare file descriptors (from parent.)
		 */
		if (flags & RFFDG) {
			FILEDESC_LOCK(p1->p_fd);
			if (p1->p_fd->fd_refcnt > 1) {
				struct filedesc *newfd;

				newfd = fdcopy(td->td_proc->p_fd);
				FILEDESC_UNLOCK(p1->p_fd);
				fdfree(td);
				p1->p_fd = newfd;
			} else
				FILEDESC_UNLOCK(p1->p_fd);
		}
		mtx_unlock(&Giant);
		*procp = NULL;
		return (0);
	}

	/*
	 * Note 1:1 allows for forking with one thread coming out on the
	 * other side with the expectation that the process is about to
	 * exec.
	 */
	if (p1->p_flag & P_SA) {
		/*
		 * Idle the other threads for a second.
		 * Since the user space is copied, it must remain stable.
		 * In addition, all threads (from the user perspective)
		 * need to either be suspended or in the kernel,
		 * where they will try restart in the parent and will
		 * be aborted in the child.
		 */
		PROC_LOCK(p1);
		if (thread_single(SINGLE_NO_EXIT)) {
			/* Abort.. someone else is single threading before us */
			PROC_UNLOCK(p1);
			mtx_unlock(&Giant);
			return (ERESTART);
		}
		PROC_UNLOCK(p1);
		/*
		 * All other activity in this process
		 * is now suspended at the user boundary,
		 * (or other safe places if we think of any).
		 */
	}

	/* Allocate new proc. */
	newproc = uma_zalloc(proc_zone, M_WAITOK);
#ifdef MAC
	mac_init_proc(newproc);
#endif

	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create.  Don't allow
	 * a nonprivileged user to use the last ten processes; don't let root
	 * exceed the limit. The variable nprocs is the current number of
	 * processes, maxproc is the limit.
	 */
	sx_xlock(&allproc_lock);
	uid = td->td_ucred->cr_ruid;
	if ((nprocs >= maxproc - 10 && uid != 0) || nprocs >= maxproc) {
		error = EAGAIN;
		goto fail;
	}

	/*
	 * Increment the count of procs running with this uid. Don't allow
	 * a nonprivileged user to exceed their current limit.
	 */
	PROC_LOCK(p1);
	ok = chgproccnt(td->td_ucred->cr_ruidinfo, 1,
		(uid != 0) ? p1->p_rlimit[RLIMIT_NPROC].rlim_cur : 0);
	PROC_UNLOCK(p1);
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
			PROC_LOCK(p2);
			while (p2->p_pid == trypid ||
			    p2->p_pgrp->pg_id == trypid ||
			    p2->p_session->s_sid == trypid) {
				trypid++;
				if (trypid >= pidchecked) {
					PROC_UNLOCK(p2);
					goto retry;
				}
			}
			if (p2->p_pid > trypid && pidchecked > p2->p_pid)
				pidchecked = p2->p_pid;
			if (p2->p_pgrp->pg_id > trypid &&
			    pidchecked > p2->p_pgrp->pg_id)
				pidchecked = p2->p_pgrp->pg_id;
			if (p2->p_session->s_sid > trypid &&
			    pidchecked > p2->p_session->s_sid)
				pidchecked = p2->p_session->s_sid;
			PROC_UNLOCK(p2);
		}
		if (!doingzomb) {
			doingzomb = 1;
			p2 = LIST_FIRST(&zombproc);
			goto again;
		}
	}

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
	LIST_INSERT_HEAD(&allproc, p2, p_list);
	LIST_INSERT_HEAD(PIDHASH(p2->p_pid), p2, p_hash);
	sx_xunlock(&allproc_lock);

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
		fd = fdinit(td->td_proc->p_fd);
		fdtol = NULL;
	} else if (flags & RFFDG) {
		FILEDESC_LOCK(p1->p_fd);
		fd = fdcopy(td->td_proc->p_fd);
		FILEDESC_UNLOCK(p1->p_fd);
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
			FILEDESC_LOCK(p1->p_fd);
			fdtol->fdl_refcount++;
			FILEDESC_UNLOCK(p1->p_fd);
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
	td2 = FIRST_THREAD_IN_PROC(p2);
	kg2 = FIRST_KSEGRP_IN_PROC(p2);
	ke2 = FIRST_KSE_IN_KSEGRP(kg2);

	/* Allocate and switch to an alternate kstack if specified */
	if (pages != 0)
		vm_thread_new_altkstack(td2, pages);

	PROC_LOCK(p2);
	PROC_LOCK(p1);

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

	bzero(&p2->p_startzero,
	    (unsigned) RANGEOF(struct proc, p_startzero, p_endzero));
	bzero(&ke2->ke_startzero,
	    (unsigned) RANGEOF(struct kse, ke_startzero, ke_endzero));
	bzero(&td2->td_startzero,
	    (unsigned) RANGEOF(struct thread, td_startzero, td_endzero));
	bzero(&kg2->kg_startzero,
	    (unsigned) RANGEOF(struct ksegrp, kg_startzero, kg_endzero));

	bcopy(&p1->p_startcopy, &p2->p_startcopy,
	    (unsigned) RANGEOF(struct proc, p_startcopy, p_endcopy));
	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    (unsigned) RANGEOF(struct thread, td_startcopy, td_endcopy));
	bcopy(&td->td_ksegrp->kg_startcopy, &kg2->kg_startcopy,
	    (unsigned) RANGEOF(struct ksegrp, kg_startcopy, kg_endcopy));
#undef RANGEOF

	/* Set up the thread as an active thread (as if runnable). */
	ke2->ke_state = KES_THREAD;
	ke2->ke_thread = td2;
	td2->td_kse = ke2;

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 * The p_stats substruct is set in vm_forkproc.
	 */
	p2->p_flag = 0;
	if (p1->p_flag & P_PROFIL)
		startprofclock(p2);
	mtx_lock_spin(&sched_lock);
	p2->p_sflag = PS_INMEM;
	/*
	 * Allow the scheduler to adjust the priority of the child and
	 * parent while we hold the sched_lock.
	 */
	sched_fork(p1, p2);

	mtx_unlock_spin(&sched_lock);
	p2->p_ucred = crhold(td->td_ucred);
	td2->td_ucred = crhold(p2->p_ucred);	/* XXXKSE */

	pargs_hold(p2->p_args);

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

	/* Bump references to the text vnode (for procfs) */
	p2->p_textvp = p1->p_textvp;
	if (p2->p_textvp)
		VREF(p2->p_textvp);
	p2->p_fd = fd;
	p2->p_fdtol = fdtol;
	PROC_UNLOCK(p1);
	PROC_UNLOCK(p2);

	/*
	 * p_limit is copy-on-write, bump refcnt,
	 */
	p2->p_limit = p1->p_limit;
	p2->p_limit->p_refcnt++;

	/*
	 * Setup linkage for kernel based threading
	 */
	if((flags & RFTHREAD) != 0) {
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
	p2->p_flag |= p1->p_flag & (P_ALTSTACK | P_SUGID);
	SESS_LOCK(p1->p_session);
	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & P_CONTROLT)
		p2->p_flag |= P_CONTROLT;
	SESS_UNLOCK(p1->p_session);
	if (flags & RFPPWAIT)
		p2->p_flag |= P_PPWAIT;

	LIST_INSERT_AFTER(p1, p2, p_pglist);
	PGRP_UNLOCK(p1->p_pgrp);
	LIST_INIT(&p2->p_children);

	callout_init(&p2->p_itcallout, CALLOUT_MPSAFE);

#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.
	 */
	mtx_lock(&ktrace_mtx);
	KASSERT(p2->p_tracevp == NULL, ("new process has a ktrace vnode"));
	if (p1->p_traceflag & KTRFAC_INHERIT) {
		p2->p_traceflag = p1->p_traceflag;
		if ((p2->p_tracevp = p1->p_tracevp) != NULL) {
			VREF(p2->p_tracevp);
			KASSERT(p1->p_tracecred != NULL,
			    ("ktrace vnode with no cred"));
			p2->p_tracecred = crhold(p1->p_tracecred);
		}
	}
	mtx_unlock(&ktrace_mtx);
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
	vm_forkproc(td, p2, td2, flags);

	if (flags == (RFFDG | RFPROC)) {
		cnt.v_forks++;
		cnt.v_forkpages += p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize;
	} else if (flags == (RFFDG | RFPROC | RFPPWAIT | RFMEM)) {
		cnt.v_vforks++;
		cnt.v_vforkpages += p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize;
	} else if (p1 == &proc0) {
		cnt.v_kthreads++;
		cnt.v_kthreadpages += p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize;
	} else {
		cnt.v_rforks++;
		cnt.v_rforkpages += p2->p_vmspace->vm_dsize +
		    p2->p_vmspace->vm_ssize;
	}

	/*
	 * Both processes are set up, now check if any loadable modules want
	 * to adjust anything.
	 *   What if they have an error? XXX
	 */
	EVENTHANDLER_INVOKE(process_fork, p1, p2, flags);

	/*
	 * If RFSTOPPED not requested, make child runnable and add to
	 * run queue.
	 */
	microuptime(&p2->p_stats->p_start);
	if ((flags & RFSTOPPED) == 0) {
		mtx_lock_spin(&sched_lock);
		p2->p_state = PRS_NORMAL;
		TD_SET_CAN_RUN(td2);
		setrunqueue(td2);
		mtx_unlock_spin(&sched_lock);
	}

	/*
	 * Now can be swapped.
	 */
	PROC_LOCK(p1);
	_PRELE(p1);

	/*
	 * Tell any interested parties about the new process.
	 */
	KNOTE(&p1->p_klist, NOTE_FORK | p2->p_pid);

	PROC_UNLOCK(p1);

	/*
	 * Preserve synchronization semantics of vfork.  If waiting for
	 * child to exec or exit, set P_PPWAIT on child, and sleep on our
	 * proc (in case of exit).
	 */
	PROC_LOCK(p2);
	while (p2->p_flag & P_PPWAIT)
		msleep(p1, &p2->p_mtx, PWAIT, "ppwait", 0);
	PROC_UNLOCK(p2);

	/*
	 * If other threads are waiting, let them continue now
	 */
	if (p1->p_flag & P_SA) {
		PROC_LOCK(p1);
		thread_single_end();
		PROC_UNLOCK(p1);
	}

	/*
	 * Return child proc pointer to parent.
	 */
	mtx_unlock(&Giant);
	*procp = p2;
	return (0);
fail:
	if (ppsratecheck(&lastfail, &curfail, 1))
		printf("maxproc limit exceeded by uid %i, please see tuning(7) and login.conf(5).\n",
			uid);
	sx_xunlock(&allproc_lock);
	uma_zfree(proc_zone, newproc);
	if (p1->p_flag & P_SA) {
		PROC_LOCK(p1);
		thread_single_end();
		PROC_UNLOCK(p1);
	}
	tsleep(&forksleep, PUSER, "fork", hz / 2);
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Handle the return of a child process from fork1().  This function
 * is called from the MD fork_trampoline() entry point.
 */
void
fork_exit(callout, arg, frame)
	void (*callout)(void *, struct trapframe *);
	void *arg;
	struct trapframe *frame;
{
	struct proc *p;
	struct thread *td;

	/*
	 * Processes normally resume in mi_switch() after being
	 * cpu_switch()'ed to, but when children start up they arrive here
	 * instead, so we must do much the same things as mi_switch() would.
	 */

	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
	td = curthread;
	p = td->td_proc;
	td->td_oncpu = PCPU_GET(cpuid);
	p->p_state = PRS_NORMAL;

	/*
	 * Finish setting up thread glue so that it begins execution in a
	 * non-nested critical section with sched_lock held but not recursed.
	 */
	sched_lock.mtx_lock = (uintptr_t)td;
	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);
	cpu_critical_fork_exit();
	CTR3(KTR_PROC, "fork_exit: new thread %p (pid %d, %s)", td, p->p_pid,
	    p->p_comm);
	mtx_unlock_spin(&sched_lock);

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
	PROC_LOCK(p);
	if (p->p_flag & P_KTHREAD) {
		PROC_UNLOCK(p);
		mtx_lock(&Giant);
		printf("Kernel thread \"%s\" (pid %d) exited prematurely.\n",
		    p->p_comm, p->p_pid);
		kthread_exit(0);
	}
	PROC_UNLOCK(p);
#ifdef DIAGNOSTIC
	cred_free_thread(td);
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
}

/*
 * Simplified back end of syscall(), used when returning from fork()
 * directly into user mode.  Giant is not held on entry, and must not
 * be held on return.  This function is passed in to fork_exit() as the
 * first parameter and is called when returning to a new userland process.
 */
void
fork_return(td, frame)
	struct thread *td;
	struct trapframe *frame;
{

	userret(td, frame, 0);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(SYS_fork, 0, 0);
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
}
