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
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/ptrace.h>
#include <sys/acct.h>		/* for acct_process() function prototype */
#include <sys/filedesc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/jail.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_zone.h>
#include <sys/user.h>

/* Required to be non-static for SysVR4 emulator */
MALLOC_DEFINE(M_ZOMBIE, "zombie", "zombie proc status");

static MALLOC_DEFINE(M_ATEXIT, "atexit", "atexit callback");

static int wait1 __P((struct thread *, struct wait_args *, int));

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
	struct proc *p = td->td_proc;
	register struct proc *q, *nq;
	register struct vmspace *vm;
	struct vnode *vtmp;
	struct exitlist *ep;

	GIANT_REQUIRED;

	if (p->p_pid == 1) {
		printf("init died (signal %d, exit %d)\n",
		    WTERMSIG(rv), WEXITSTATUS(rv));
		panic("Going nowhere without my init!");
	}

/* XXXXKSE */
/* MUST abort all other threads before proceeding past this point */

	/* are we a task leader? */
	PROC_LOCK(p);
	if(p == p->p_leader) {
		q = p->p_peers;
		while (q != NULL) {
			PROC_LOCK(q);
			psignal(q, SIGKILL);
			PROC_UNLOCK(q);
			q = q->p_peers;
		}
		while (p->p_peers) 
			msleep((caddr_t)p, &p->p_mtx, PWAIT, "exit1", 0);
	}
	PROC_UNLOCK(p);

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

	stopprofclock(p);

	MALLOC(p->p_ru, struct rusage *, sizeof(struct rusage),
		M_ZOMBIE, M_WAITOK);
	/*
	 * If parent is waiting for us to exit or exec,
	 * P_PPWAIT is set; we will wakeup the parent below.
	 */
	PROC_LOCK(p);
	p->p_flag &= ~(P_TRACED | P_PPWAIT);
	p->p_flag |= P_WEXIT;
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
	fdfree(td); /* XXXKSE *//* may not be the one in proc */

	/*
	 * Remove ourself from our leader's peer list and wake our leader.
	 */
	PROC_LOCK(p->p_leader);
	if(p->p_leader->p_peers) {
		q = p->p_leader;
		while(q->p_peers != p)
			q = q->p_peers;
		q->p_peers = p->p_peers;
		wakeup((caddr_t)p->p_leader);
	}
	PROC_UNLOCK(p->p_leader);

	/* The next two chunks should probably be moved to vmspace_exit. */
	vm = p->p_vmspace;
	/*
	 * Release user portion of address space.
	 * This releases references to vnodes,
	 * which could cause I/O if the file has been unlinked.
	 * Need to do this early enough that we can still sleep.
	 * Can't free the entire vmspace as the kernel stack
	 * may be mapped within that space also.
	 */
	if (vm->vm_refcnt == 1) {
		if (vm->vm_shm)
			shmexit(p);
		pmap_remove_pages(vmspace_pmap(vm), VM_MIN_ADDRESS,
		    VM_MAXUSER_ADDRESS);
		(void) vm_map_remove(&vm->vm_map, VM_MIN_ADDRESS,
		    VM_MAXUSER_ADDRESS);
	}

	PROC_LOCK(p);
	if (SESS_LEADER(p)) {
		register struct session *sp = p->p_session;

		PROC_UNLOCK(p);
		if (sp->s_ttyvp) {
			/*
			 * Controlling process.
			 * Signal foreground pgrp,
			 * drain controlling terminal
			 * and revoke access to controlling terminal.
			 */
			if (sp->s_ttyp && (sp->s_ttyp->t_session == sp)) {
				if (sp->s_ttyp->t_pgrp)
					pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);
				(void) ttywait(sp->s_ttyp);
				/*
				 * The tty could have been revoked
				 * if we blocked.
				 */
				if (sp->s_ttyvp)
					VOP_REVOKE(sp->s_ttyvp, REVOKEALL);
			}
			if (sp->s_ttyvp)
				vrele(sp->s_ttyvp);
			sp->s_ttyvp = NULL;
			/*
			 * s_ttyp is not zero'd; we use this to indicate
			 * that the session once had a controlling terminal.
			 * (for logging and informational purposes)
			 */
		}
		sp->s_leader = NULL;
	} else
		PROC_UNLOCK(p);
	fixjobc(p, p->p_pgrp, 0);
	(void)acct_process(td);
#ifdef KTRACE
	/*
	 * release trace file
	 */
	p->p_traceflag = 0;	/* don't trace the vrele() */
	if ((vtmp = p->p_tracep) != NULL) {
		p->p_tracep = NULL;
		vrele(vtmp);
	}
#endif
	/*
	 * Release reference to text vnode
	 */
	if ((vtmp = p->p_textvp) != NULL) {
		p->p_textvp = NULL;
		vrele(vtmp);
	}

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
		wakeup((caddr_t) initproc);
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
	p->p_xstat = rv;
	*p->p_ru = p->p_stats->p_ru;
	mtx_lock_spin(&sched_lock);
	calcru(p, &p->p_ru->ru_utime, &p->p_ru->ru_stime, NULL);
	mtx_unlock_spin(&sched_lock);
	ruadd(p->p_ru, &p->p_stats->p_cru);

	/*
	 * Pretend that an mi_switch() to the next process occurs now.  We
	 * must set `switchtime' directly since we will call cpu_switch()
	 * directly.  Set it now so that the rest of the exit time gets
	 * counted somewhere if possible.
	 */
	mtx_lock_spin(&sched_lock);
	microuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);
	mtx_unlock_spin(&sched_lock);

	/*
	 * notify interested parties of our demise.
	 */
	PROC_LOCK(p);
	KNOTE(&p->p_klist, NOTE_EXIT);

	/*
	 * Notify parent that we're gone.  If parent has the PS_NOCLDWAIT
	 * flag set, notify process 1 instead (and hope it will handle
	 * this situation).
	 */
	if (p->p_pptr->p_procsig->ps_flag & PS_NOCLDWAIT) {
		struct proc *pp = p->p_pptr;
		proc_reparent(p, initproc);
		/*
		 * If this was the last child of our parent, notify
		 * parent, so in case he was wait(2)ing, he will
		 * continue.
		 */
		if (LIST_EMPTY(&pp->p_children))
			wakeup((caddr_t)pp);
	}

	PROC_LOCK(p->p_pptr);
	if (p->p_sigparent && p->p_pptr != initproc)
	        psignal(p->p_pptr, p->p_sigparent);
	else
	        psignal(p->p_pptr, SIGCHLD);
	PROC_UNLOCK(p->p_pptr);

	/*
	 * If this is a kthread, then wakeup anyone waiting for it to exit.
	 */
	if (p->p_flag & P_KTHREAD)
		wakeup((caddr_t)p);
	PROC_UNLOCK(p);
	sx_xunlock(&proctree_lock);
	
	/*
	 * Clear curproc after we've done all operations
	 * that could block, and before tearing down the rest
	 * of the process state that might be used from clock, etc.
	 * Also, can't clear curproc while we're still runnable,
	 * as we're not on a run queue (we are current, just not
	 * a proper proc any longer!).
	 *
	 * Other substructures are freed from wait().
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
	 * Finally, call machine-dependent code to release the remaining
	 * resources including address space, the kernel stack and pcb.
	 * The address space is released by "vmspace_free(p->p_vmspace)"
	 * in vm_waitproc();
	 */
	cpu_exit(td);

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	while (mtx_owned(&Giant))
		mtx_unlock(&Giant);

	/*
	 * We have to wait until after releasing all locks before
	 * changing p_stat.  If we block on a mutex then we will be
	 * back at SRUN when we resume and our parent will never
	 * harvest us.
	 */
	p->p_stat = SZOMB;

	wakeup(p->p_pptr);
	PROC_UNLOCK(p);

	cnt.v_swtch++;
	cpu_throw();
	panic("exit1");
}

#ifdef COMPAT_43
/*
 * MPSAFE, the dirty work is handled by wait1().
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
 * MPSAFE, the dirty work is handled by wait1().
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
	register int nfound;
	register struct proc *q, *p, *t;
	int status, error;

	mtx_lock(&Giant);
	q = td->td_proc;
	if (uap->pid == 0)
		uap->pid = -q->p_pgid;
	if (uap->options &~ (WUNTRACED|WNOHANG|WLINUXCLONE)) {
		error = EINVAL;
		goto done2;
	}
loop:
	nfound = 0;
	sx_slock(&proctree_lock);
	LIST_FOREACH(p, &q->p_children, p_sibling) {
		if (uap->pid != WAIT_ANY &&
		    p->p_pid != uap->pid && p->p_pgid != -uap->pid)
			continue;

		/*
		 * This special case handles a kthread spawned by linux_clone 
		 * (see linux_misc.c).  The linux_wait4 and linux_waitpid
		 * functions need to be able to distinguish between waiting
		 * on a process and waiting on a thread.  It is a thread if
		 * p_sigparent is not SIGCHLD, and the WLINUXCLONE option
		 * signifies we want to wait for threads and not processes.
		 */
		PROC_LOCK(p);
		if ((p->p_sigparent != SIGCHLD) ^
		    ((uap->options & WLINUXCLONE) != 0)) {
			PROC_UNLOCK(p);
			continue;
		}

		nfound++;
		mtx_lock_spin(&sched_lock);
		if (p->p_stat == SZOMB) {
			/*
			 * charge childs scheduling cpu usage to parent
			 * XXXKSE assume only one thread & kse & ksegrp
			 * keep estcpu in each ksegrp
			 * so charge it to the ksegrp that did the wait
			 * since process estcpu is sum of all ksegrps,
			 * this is strictly as expected.
			 * Assume that the child process aggregated all 
			 * tke estcpu into the 'build-in' ksegrp.
			 * XXXKSE
			 */
			if (curthread->td_proc->p_pid != 1) {
				curthread->td_ksegrp->kg_estcpu =
				    ESTCPULIM(curthread->td_ksegrp->kg_estcpu +
				    p->p_ksegrp.kg_estcpu);
			}

			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);

			td->td_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat)
				td->td_retval[1] = p->p_xstat;
			else
#endif
			if (uap->status) {
				status = p->p_xstat;	/* convert to int */
				if ((error = copyout((caddr_t)&status,
				    (caddr_t)uap->status, sizeof(status)))) {
					goto done2;
				}
			}
			if (uap->rusage && (error = copyout((caddr_t)p->p_ru,
			    (caddr_t)uap->rusage, sizeof (struct rusage)))) {
				goto done2;
			}
			/*
			 * If we got the child via a ptrace 'attach',
			 * we need to give it back to the old parent.
			 */
			sx_xlock(&proctree_lock);
			if (p->p_oppid) {
				if ((t = pfind(p->p_oppid)) != NULL) {
					PROC_LOCK(p);
					p->p_oppid = 0;
					proc_reparent(p, t);
					PROC_UNLOCK(p);
					psignal(t, SIGCHLD);
					wakeup((caddr_t)t);
					PROC_UNLOCK(t);
					sx_xunlock(&proctree_lock);
					error = 0;
					goto done2;
				}
			}
			sx_xunlock(&proctree_lock);
			PROC_LOCK(p);
			p->p_xstat = 0;
			PROC_UNLOCK(p);
			ruadd(&q->p_stats->p_cru, p->p_ru);
			FREE(p->p_ru, M_ZOMBIE);
			p->p_ru = NULL;

			/*
			 * Decrement the count of procs running with this uid.
			 */
			(void)chgproccnt(p->p_ucred->cr_ruidinfo, -1, 0);

			/*
			 * Finally finished with old proc entry.
			 * Unlink it from its process group and free it.
			 */
			leavepgrp(p);

			sx_xlock(&allproc_lock);
			LIST_REMOVE(p, p_list);	/* off zombproc */
			sx_xunlock(&allproc_lock);

			sx_xlock(&proctree_lock);
			LIST_REMOVE(p, p_sibling);
			sx_xunlock(&proctree_lock);

			/*
			 * Free up credentials.
			 */
			crfree(p->p_ucred);
			p->p_ucred = NULL;

			/*
			 * Remove unused arguments
			 */
			if (p->p_args && --p->p_args->ar_ref == 0)
				FREE(p->p_args, M_PARGS);

			if (--p->p_procsig->ps_refcnt == 0) {
				if (p->p_sigacts != &p->p_uarea->u_sigacts)
					FREE(p->p_sigacts, M_SUBPROC);
			        FREE(p->p_procsig, M_SUBPROC);
				p->p_procsig = NULL;
			}

			/*
			 * Give vm and machine-dependent layer a chance
			 * to free anything that cpu_exit couldn't
			 * release while still running in process context.
			 */
			vm_waitproc(p);
			mtx_destroy(&p->p_mtx);
			zfree(proc_zone, p);
			nprocs--;
			error = 0;
			goto done2;
		}
		if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED || uap->options & WUNTRACED)) {
			mtx_unlock_spin(&sched_lock);
			p->p_flag |= P_WAITED;
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);
			td->td_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat) {
				td->td_retval[1] = W_STOPCODE(p->p_xstat);
				error = 0;
			} else
#endif
			if (uap->status) {
				status = W_STOPCODE(p->p_xstat);
				error = copyout((caddr_t)&status,
					(caddr_t)uap->status, sizeof(status));
			} else
				error = 0;
			goto done2;
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	}
	sx_sunlock(&proctree_lock);
	if (nfound == 0) {
		error = ECHILD;
		goto done2;
	}
	if (uap->options & WNOHANG) {
		td->td_retval[0] = 0;
		error = 0;
		goto done2;
	}
	if ((error = tsleep((caddr_t)q, PWAIT | PCATCH, "wait", 0)) != 0)
		goto done2;
	goto loop;
done2:
	mtx_unlock(&Giant);
	return(error);
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
			return(1);
		}
	}	
	return (0);
}
