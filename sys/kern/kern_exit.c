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
 *	@(#)kern_exit.c	8.7 (Berkeley) 2/12/94
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_ktrace.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/tty.h>
#include <sys/wait.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/ptrace.h>
#include <sys/acct.h>		/* for acct_process() function prototype */
#include <sys/filedesc.h>
#include <sys/mac.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/jail.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <sys/user.h>

/* Required to be non-static for SysVR4 emulator */
MALLOC_DEFINE(M_ZOMBIE, "zombie", "zombie proc status");

static MALLOC_DEFINE(M_ATEXIT, "atexit", "atexit callback");

static int wait1(struct thread *, struct wait_args *, int);

/*
 * callout list for things to do at exit time
 */
struct exitlist {
	exitlist_fn function;
	TAILQ_ENTRY(exitlist) next;
};

TAILQ_HEAD(exit_list_head, exitlist);
static struct exit_list_head exit_list = TAILQ_HEAD_INITIALIZER(exit_list);

/*
 * exit --
 *	Death of process.
 *
 * MPSAFE
 */
void
sys_exit(td, uap)
	struct thread *td;
	struct sys_exit_args /* {
		int	rval;
	} */ *uap;
{

	mtx_lock(&Giant);
	exit1(td, W_EXITCODE(uap->rval, 0));
	/* NOTREACHED */
}

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(td, rv)
	register struct thread *td;
	int rv;
{
	struct exitlist *ep;
	struct proc *p, *nq, *q;
	struct tty *tp;
	struct vnode *ttyvp;
	register struct vmspace *vm;
	struct vnode *vtmp;
#ifdef KTRACE
	struct vnode *tracevp;
#endif

	GIANT_REQUIRED;

	p = td->td_proc;
	if (p == initproc) {
		printf("init died (signal %d, exit %d)\n",
		    WTERMSIG(rv), WEXITSTATUS(rv));
		panic("Going nowhere without my init!");
	}

	/*
	 * XXXKSE: MUST abort all other threads before proceeding past here.
	 */
	PROC_LOCK(p);
	if (p->p_flag & P_KSES) {
		/*
		 * First check if some other thread got here before us..
		 * if so, act apropriatly, (exit or suspend);
		 */
		thread_suspend_check(0);

		/*
		 * Kill off the other threads. This requires
		 * Some co-operation from other parts of the kernel
		 * so it may not be instant.
		 * With this state set:
		 * Any thread entering the kernel from userspace will
		 * thread_exit() in trap().  Any thread attempting to
		 * sleep will return immediatly
		 * with EINTR or EWOULDBLOCK, which will hopefully force them
		 * to back out to userland, freeing resources as they go, and
		 * anything attempting to return to userland will thread_exit()
		 * from userret().  thread_exit() will unsuspend us
		 * when the last other thread exits.
		 */
		if (thread_single(SINGLE_EXIT)) {
			panic ("Exit: Single threading fouled up");
		}
		/*
		 * All other activity in this process is now stopped.
		 * Remove excess KSEs and KSEGRPS. XXXKSE (when we have them)
		 * ... 
		 * Turn off threading support.
		 */
		p->p_flag &= ~P_KSES;
		thread_single_end(); 	/* Don't need this any more. */
	}
	/*
	 * With this state set:
	 * Any thread entering the kernel from userspace will thread_exit()
	 * in trap().  Any thread attempting to sleep will return immediatly
	 * with EINTR or EWOULDBLOCK, which will hopefully force them
	 * to back out to userland, freeing resources as they go, and
	 * anything attempting to return to userland will thread_exit()
	 * from userret().  thread_exit() will do a wakeup on p->p_numthreads
	 * if it transitions to 1.
	 */

	p->p_flag |= P_WEXIT;
	PROC_UNLOCK(p);

	/* Are we a task leader? */
	if (p == p->p_leader) {
		mtx_lock(&ppeers_lock);
		q = p->p_peers;
		while (q != NULL) {
			PROC_LOCK(q);
			psignal(q, SIGKILL);
			PROC_UNLOCK(q);
			q = q->p_peers;
		}
		while (p->p_peers != NULL) 
			msleep(p, &ppeers_lock, PWAIT, "exit1", 0);
		mtx_unlock(&ppeers_lock);
	}

#ifdef PGINPROF
	vmsizmon();
#endif
	STOPEVENT(p, S_EXIT, rv);
	wakeup(&p->p_stype);	/* Wakeup anyone in procfs' PIOCWAIT */

	/* 
	 * Check if any loadable modules need anything done at process exit.
	 * e.g. SYSV IPC stuff
	 * XXX what if one of these generates an error?
	 */
	TAILQ_FOREACH(ep, &exit_list, next) 
		(*ep->function)(p);


	MALLOC(p->p_ru, struct rusage *, sizeof(struct rusage),
		M_ZOMBIE, 0);
	/*
	 * If parent is waiting for us to exit or exec,
	 * P_PPWAIT is set; we will wakeup the parent below.
	 */
	PROC_LOCK(p);
	stopprofclock(p);
	p->p_flag &= ~(P_TRACED | P_PPWAIT);
	SIGEMPTYSET(p->p_siglist);
	PROC_UNLOCK(p);
	if (timevalisset(&p->p_realtimer.it_value))
		callout_stop(&p->p_itcallout);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pid.
	 */
	funsetownlst(&p->p_sigiolst);

	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
	fdfree(td);

	/*
	 * Remove ourself from our leader's peer list and wake our leader.
	 */
	mtx_lock(&ppeers_lock);
	if (p->p_leader->p_peers) {
		q = p->p_leader;
		while (q->p_peers != p)
			q = q->p_peers;
		q->p_peers = p->p_peers;
		wakeup(p->p_leader);
	}
	mtx_unlock(&ppeers_lock);

	/* The next two chunks should probably be moved to vmspace_exit. */
	vm = p->p_vmspace;
	/*
	 * Release user portion of address space.
	 * This releases references to vnodes,
	 * which could cause I/O if the file has been unlinked.
	 * Need to do this early enough that we can still sleep.
	 * Can't free the entire vmspace as the kernel stack
	 * may be mapped within that space also.
	 *
	 * Processes sharing the same vmspace may exit in one order, and
	 * get cleaned up by vmspace_exit() in a different order.  The
	 * last exiting process to reach this point releases as much of
	 * the environment as it can, and the last process cleaned up
	 * by vmspace_exit() (which decrements exitingcnt) cleans up the
	 * remainder.
	 */
	++vm->vm_exitingcnt;
	if (--vm->vm_refcnt == 0) {
		shmexit(vm);
		vm_page_lock_queues();
		pmap_remove_pages(vmspace_pmap(vm), vm_map_min(&vm->vm_map),
		    vm_map_max(&vm->vm_map));
		vm_page_unlock_queues();
		(void) vm_map_remove(&vm->vm_map, vm_map_min(&vm->vm_map),
		    vm_map_max(&vm->vm_map));
	}

	sx_xlock(&proctree_lock);
	if (SESS_LEADER(p)) {
		register struct session *sp;

		sp = p->p_session;
		if (sp->s_ttyvp) {
			/*
			 * Controlling process.
			 * Signal foreground pgrp,
			 * drain controlling terminal
			 * and revoke access to controlling terminal.
			 */
			if (sp->s_ttyp && (sp->s_ttyp->t_session == sp)) {
				tp = sp->s_ttyp;
				if (sp->s_ttyp->t_pgrp) {
					PGRP_LOCK(sp->s_ttyp->t_pgrp);
					pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);
					PGRP_UNLOCK(sp->s_ttyp->t_pgrp);
				}
				/* XXX tp should be locked. */
				sx_xunlock(&proctree_lock);
				(void) ttywait(tp);
				sx_xlock(&proctree_lock);
				/*
				 * The tty could have been revoked
				 * if we blocked.
				 */
				if (sp->s_ttyvp) {
					ttyvp = sp->s_ttyvp;
					SESS_LOCK(p->p_session);
					sp->s_ttyvp = NULL;
					SESS_UNLOCK(p->p_session);
					sx_xunlock(&proctree_lock);
					VOP_REVOKE(ttyvp, REVOKEALL);
					vrele(ttyvp);
					sx_xlock(&proctree_lock);
				}
			}
			if (sp->s_ttyvp) {
				ttyvp = sp->s_ttyvp;
				SESS_LOCK(p->p_session);
				sp->s_ttyvp = NULL;
				SESS_UNLOCK(p->p_session);
				vrele(ttyvp);
			}
			/*
			 * s_ttyp is not zero'd; we use this to indicate
			 * that the session once had a controlling terminal.
			 * (for logging and informational purposes)
			 */
		}
		SESS_LOCK(p->p_session);
		sp->s_leader = NULL;
		SESS_UNLOCK(p->p_session);
	}
	fixjobc(p, p->p_pgrp, 0);
	sx_xunlock(&proctree_lock);
	(void)acct_process(td);
#ifdef KTRACE
	/*
	 * release trace file
	 */
	PROC_LOCK(p);
	mtx_lock(&ktrace_mtx);
	p->p_traceflag = 0;	/* don't trace the vrele() */
	tracevp = p->p_tracep;
	p->p_tracep = NULL;
	mtx_unlock(&ktrace_mtx);
	PROC_UNLOCK(p);
	if (tracevp != NULL)
		vrele(tracevp);
#endif
	/*
	 * Release reference to text vnode
	 */
	if ((vtmp = p->p_textvp) != NULL) {
		p->p_textvp = NULL;
		vrele(vtmp);
	}

	/*
	 * Release our limits structure.
	 */
	mtx_assert(&Giant, MA_OWNED);
	if (--p->p_limit->p_refcnt == 0) {
		FREE(p->p_limit, M_SUBPROC);
		p->p_limit = NULL;
	}

	/*
	 * Release this thread's reference to the ucred.  The actual proc
	 * reference will stay around until the proc is harvested by
	 * wait().  At this point the ucred is immutable (no other threads
	 * from this proc are around that can change it) so we leave the
	 * per-thread ucred pointer intact in case it is needed although
	 * in theory nothing should be using it at this point.
	 */
	crfree(td->td_ucred);

	/*
	 * Remove proc from allproc queue and pidhash chain.
	 * Place onto zombproc.  Unlink from parent's child list.
	 */
	sx_xlock(&allproc_lock);
	LIST_REMOVE(p, p_list);
	LIST_INSERT_HEAD(&zombproc, p, p_list);
	LIST_REMOVE(p, p_hash);
	sx_xunlock(&allproc_lock);

	sx_xlock(&proctree_lock);
	q = LIST_FIRST(&p->p_children);
	if (q != NULL)		/* only need this if any child is S_ZOMB */
		wakeup(initproc);
	for (; q != NULL; q = nq) {
		nq = LIST_NEXT(q, p_sibling);
		PROC_LOCK(q);
		proc_reparent(q, initproc);
		q->p_sigparent = SIGCHLD;
		/*
		 * Traced processes are killed
		 * since their existence means someone is screwing up.
		 */
		if (q->p_flag & P_TRACED) {
			q->p_flag &= ~P_TRACED;
			psignal(q, SIGKILL);
		}
		PROC_UNLOCK(q);
	}

	/*
	 * Save exit status and final rusage info, adding in child rusage
	 * info and self times.
	 */
	PROC_LOCK(p);
	p->p_xstat = rv;
	*p->p_ru = p->p_stats->p_ru;
	mtx_lock_spin(&sched_lock);
	calcru(p, &p->p_ru->ru_utime, &p->p_ru->ru_stime, NULL);
	mtx_unlock_spin(&sched_lock);
	ruadd(p->p_ru, &p->p_stats->p_cru);

	/*
	 * Notify interested parties of our demise.
	 */
	KNOTE(&p->p_klist, NOTE_EXIT);

	/*
	 * Notify parent that we're gone.  If parent has the PS_NOCLDWAIT
	 * flag set, or if the handler is set to SIG_IGN, notify process
	 * 1 instead (and hope it will handle this situation).
	 */
	PROC_LOCK(p->p_pptr);
	if (p->p_pptr->p_procsig->ps_flag & (PS_NOCLDWAIT | PS_CLDSIGIGN)) {
		struct proc *pp;

		pp = p->p_pptr;
		PROC_UNLOCK(pp);
		proc_reparent(p, initproc);
		PROC_LOCK(p->p_pptr);
		/*
		 * If this was the last child of our parent, notify
		 * parent, so in case he was wait(2)ing, he will
		 * continue.
		 */
		if (LIST_EMPTY(&pp->p_children))
			wakeup(pp);
	}

	if (p->p_sigparent && p->p_pptr != initproc)
		psignal(p->p_pptr, p->p_sigparent);
	else
		psignal(p->p_pptr, SIGCHLD);
	PROC_UNLOCK(p->p_pptr);

	/*
	 * If this is a kthread, then wakeup anyone waiting for it to exit.
	 */
	if (p->p_flag & P_KTHREAD)
		wakeup(p);
	PROC_UNLOCK(p);
	
	/*
	 * Finally, call machine-dependent code to release the remaining
	 * resources including address space.
	 * The address space is released by "vmspace_exitfree(p)" in
	 * vm_waitproc().
	 */
	cpu_exit(td);

	PROC_LOCK(p);
	PROC_LOCK(p->p_pptr);
	sx_xunlock(&proctree_lock);
	mtx_lock_spin(&sched_lock);

	while (mtx_owned(&Giant))
		mtx_unlock(&Giant);

	/*
	 * We have to wait until after releasing all locks before
	 * changing p_state.  If we block on a mutex then we will be
	 * back at SRUN when we resume and our parent will never
	 * harvest us.
	 */
	p->p_state = PRS_ZOMBIE;

	wakeup(p->p_pptr);
	PROC_UNLOCK(p->p_pptr);
	cnt.v_swtch++;
	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	cpu_sched_exit(td); /* XXXKSE check if this should be in thread_exit */
	/*
	 * Make sure the scheduler takes this thread out of its tables etc.
	 * This will also release this thread's reference to the ucred.
 	 * Other thread parts to release include pcb bits and such.
	 */
	thread_exit();
}

#ifdef COMPAT_43
/*
 * MPSAFE.  The dirty work is handled by wait1().
 */
int
owait(td, uap)
	struct thread *td;
	register struct owait_args /* {
		int     dummy;
	} */ *uap;
{
	struct wait_args w;

	w.options = 0;
	w.rusage = NULL;
	w.pid = WAIT_ANY;
	w.status = NULL;
	return (wait1(td, &w, 1));
}
#endif /* COMPAT_43 */

/*
 * MPSAFE.  The dirty work is handled by wait1().
 */
int
wait4(td, uap)
	struct thread *td;
	struct wait_args *uap;
{

	return (wait1(td, uap, 0));
}

/*
 * MPSAFE
 */
static int
wait1(td, uap, compat)
	register struct thread *td;
	register struct wait_args /* {
		int pid;
		int *status;
		int options;
		struct rusage *rusage;
	} */ *uap;
	int compat;
{
	struct rusage ru;
	int nfound;
	struct proc *p, *q, *t;
	int status, error;

	q = td->td_proc;
	if (uap->pid == 0) {
		PROC_LOCK(q);
		uap->pid = -q->p_pgid;
		PROC_UNLOCK(q);
	}
	if (uap->options &~ (WUNTRACED|WNOHANG|WCONTINUED|WLINUXCLONE))
		return (EINVAL);
	mtx_lock(&Giant);
loop:
	nfound = 0;
	sx_xlock(&proctree_lock);
	LIST_FOREACH(p, &q->p_children, p_sibling) {
		PROC_LOCK(p);
		if (uap->pid != WAIT_ANY &&
		    p->p_pid != uap->pid && p->p_pgid != -uap->pid) {
			PROC_UNLOCK(p);
			continue;
		}

		/*
		 * This special case handles a kthread spawned by linux_clone 
		 * (see linux_misc.c).  The linux_wait4 and linux_waitpid
		 * functions need to be able to distinguish between waiting
		 * on a process and waiting on a thread.  It is a thread if
		 * p_sigparent is not SIGCHLD, and the WLINUXCLONE option
		 * signifies we want to wait for threads and not processes.
		 */
		if ((p->p_sigparent != SIGCHLD) ^
		    ((uap->options & WLINUXCLONE) != 0)) {
			PROC_UNLOCK(p);
			continue;
		}

		nfound++;
		if (p->p_state == PRS_ZOMBIE) {
			/*
			 * Allow the scheduler to adjust the priority of the
			 * parent when a kseg is exiting.
			 */
			if (curthread->td_proc->p_pid != 1) {
				mtx_lock_spin(&sched_lock);
				sched_exit(curthread->td_ksegrp,
				    FIRST_KSEGRP_IN_PROC(p));
				mtx_unlock_spin(&sched_lock);
			}

			td->td_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat)
				td->td_retval[1] = p->p_xstat;
			else
#endif
			if (uap->status) {
				status = p->p_xstat;	/* convert to int */
				PROC_UNLOCK(p);
				if ((error = copyout(&status,
				    uap->status, sizeof(status)))) {
					sx_xunlock(&proctree_lock);
					mtx_unlock(&Giant);
					return (error);
				}
				PROC_LOCK(p);
			}
			if (uap->rusage) {
				bcopy(p->p_ru, &ru, sizeof(ru));
				PROC_UNLOCK(p);
				if ((error = copyout(&ru,
				    uap->rusage, sizeof (struct rusage)))) {
					sx_xunlock(&proctree_lock);
					mtx_unlock(&Giant);
					return (error);
				}
			} else
				PROC_UNLOCK(p);
			/*
			 * If we got the child via a ptrace 'attach',
			 * we need to give it back to the old parent.
			 */
			if (p->p_oppid && (t = pfind(p->p_oppid)) != NULL) {
				PROC_LOCK(p);
				p->p_oppid = 0;
				proc_reparent(p, t);
				PROC_UNLOCK(p);
				psignal(t, SIGCHLD);
				wakeup(t);
				PROC_UNLOCK(t);
				sx_xunlock(&proctree_lock);
				mtx_unlock(&Giant);
				return (0);
			}
			/*
			 * Remove other references to this process to ensure
			 * we have an exclusive reference.
			 */
			leavepgrp(p);

			sx_xlock(&allproc_lock);
			LIST_REMOVE(p, p_list);	/* off zombproc */
			sx_xunlock(&allproc_lock);

			LIST_REMOVE(p, p_sibling);
			sx_xunlock(&proctree_lock);

			/*
			 * As a side effect of this lock, we know that
			 * all other writes to this proc are visible now, so
			 * no more locking is needed for p.
			 */
			PROC_LOCK(p);
			p->p_xstat = 0;		/* XXX: why? */
			PROC_UNLOCK(p);
			PROC_LOCK(q);
			ruadd(&q->p_stats->p_cru, p->p_ru);
			PROC_UNLOCK(q);
			FREE(p->p_ru, M_ZOMBIE);
			p->p_ru = NULL;

			/*
			 * Decrement the count of procs running with this uid.
			 */
			(void)chgproccnt(p->p_ucred->cr_ruidinfo, -1, 0);

			/*
			 * Free up credentials.
			 */
			crfree(p->p_ucred);
			p->p_ucred = NULL;	/* XXX: why? */

			/*
			 * Remove unused arguments
			 */
			pargs_drop(p->p_args);
			p->p_args = NULL;

			if (--p->p_procsig->ps_refcnt == 0) {
				if (p->p_sigacts != &p->p_uarea->u_sigacts)
					FREE(p->p_sigacts, M_SUBPROC);
				FREE(p->p_procsig, M_SUBPROC);
				p->p_procsig = NULL;
			}

			/*
			 * do any thread-system specific cleanups
			 */
			thread_wait(p);

			/*
			 * Give vm and machine-dependent layer a chance
			 * to free anything that cpu_exit couldn't
			 * release while still running in process context.
			 */
			vm_waitproc(p);
			mtx_destroy(&p->p_mtx);
#ifdef MAC
			mac_destroy_proc(p);
#endif
			KASSERT(FIRST_THREAD_IN_PROC(p),
			    ("wait1: no residual thread!"));
			uma_zfree(proc_zone, p);
			sx_xlock(&allproc_lock);
			nprocs--;
			sx_xunlock(&allproc_lock);
			mtx_unlock(&Giant);
			return (0);
		}
		if (P_SHOULDSTOP(p) && ((p->p_flag & P_WAITED) == 0) &&
		    (p->p_flag & P_TRACED || uap->options & WUNTRACED)) {
			p->p_flag |= P_WAITED;
			sx_xunlock(&proctree_lock);
			td->td_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat) {
				td->td_retval[1] = W_STOPCODE(p->p_xstat);
				PROC_UNLOCK(p);
				error = 0;
			} else
#endif
			if (uap->status) {
				status = W_STOPCODE(p->p_xstat);
				PROC_UNLOCK(p);
				error = copyout(&status,
					uap->status, sizeof(status));
			} else {
				PROC_UNLOCK(p);
				error = 0;
			}
			mtx_unlock(&Giant);
			return (error);
		}
		if (uap->options & WCONTINUED && (p->p_flag & P_CONTINUED)) {
			sx_xunlock(&proctree_lock);
			td->td_retval[0] = p->p_pid;
			p->p_flag &= ~P_CONTINUED;
			PROC_UNLOCK(p);

			if (uap->status) {
				status = SIGCONT;
				error = copyout(&status,
				    uap->status, sizeof(status));
			} else
				error = 0;

			mtx_unlock(&Giant);
			return (error);
		}
		PROC_UNLOCK(p);
	}
	if (nfound == 0) {
		sx_xunlock(&proctree_lock);
		mtx_unlock(&Giant);
		return (ECHILD);
	}
	if (uap->options & WNOHANG) {
		sx_xunlock(&proctree_lock);
		td->td_retval[0] = 0;
		mtx_unlock(&Giant);
		return (0);
	}
	PROC_LOCK(q);
	sx_xunlock(&proctree_lock);
	error = msleep(q, &q->p_mtx, PWAIT | PCATCH, "wait", 0);
	PROC_UNLOCK(q);
	if (error) {
		mtx_unlock(&Giant);
		return (error);
	}
	goto loop;
}

/*
 * Make process 'parent' the new parent of process 'child'.
 * Must be called with an exclusive hold of proctree lock.
 */
void
proc_reparent(child, parent)
	register struct proc *child;
	register struct proc *parent;
{

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(child, MA_OWNED);
	if (child->p_pptr == parent)
		return;

	LIST_REMOVE(child, p_sibling);
	LIST_INSERT_HEAD(&parent->p_children, child, p_sibling);
	child->p_pptr = parent;
}

/*
 * The next two functions are to handle adding/deleting items on the
 * exit callout list
 * 
 * at_exit():
 * Take the arguments given and put them onto the exit callout list,
 * However first make sure that it's not already there.
 * returns 0 on success.
 */

int
at_exit(function)
	exitlist_fn function;
{
	struct exitlist *ep;

#ifdef INVARIANTS
	/* Be noisy if the programmer has lost track of things */
	if (rm_at_exit(function)) 
		printf("WARNING: exit callout entry (%p) already present\n",
		    function);
#endif
	ep = malloc(sizeof(*ep), M_ATEXIT, M_NOWAIT);
	if (ep == NULL)
		return (ENOMEM);
	ep->function = function;
	TAILQ_INSERT_TAIL(&exit_list, ep, next);
	return (0);
}

/*
 * Scan the exit callout list for the given item and remove it.
 * Returns the number of items removed (0 or 1)
 */
int
rm_at_exit(function)
	exitlist_fn function;
{
	struct exitlist *ep;

	TAILQ_FOREACH(ep, &exit_list, next) {
		if (ep->function == function) {
			TAILQ_REMOVE(&exit_list, ep, next);
			free(ep, M_ATEXIT);
			return (1);
		}
	}
	return (0);
}
