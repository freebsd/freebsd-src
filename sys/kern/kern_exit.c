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
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/tty.h>
#include <sys/wait.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/ptrace.h>
#include <sys/acct.h>		/* for acct_process() function prototype */
#include <sys/filedesc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/aio.h>
#include <sys/jail.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_zone.h>
#include <sys/user.h>

/* Required to be non-static for SysVR4 emulator */
MALLOC_DEFINE(M_ZOMBIE, "zombie", "zombie proc status");

static MALLOC_DEFINE(M_ATEXIT, "atexit", "atexit callback");

static int wait1 __P((struct proc *, struct wait_args *, int));

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
 */
void
exit(p, uap)
	struct proc *p;
	struct rexit_args /* {
		int	rval;
	} */ *uap;
{

	exit1(p, W_EXITCODE(uap->rval, 0));
	/* NOTREACHED */
}

/*
 * Exit: deallocate address space and other resources, change proc state
 * to zombie, and unlink proc from allproc and parent's lists.  Save exit
 * status and rusage for wait().  Check for child processes and orphan them.
 */
void
exit1(p, rv)
	register struct proc *p;
	int rv;
{
	register struct proc *q, *nq;
	register struct vmspace *vm;
	struct exitlist *ep;

	if (p->p_pid == 1) {
		printf("init died (signal %d, exit %d)\n",
		    WTERMSIG(rv), WEXITSTATUS(rv));
		panic("Going nowhere without my init!");
	}

	aio_proc_rundown(p);

	/* are we a task leader? */
	if(p == p->p_leader) {
        	struct kill_args killArgs;
		killArgs.signum = SIGKILL;
		q = p->p_peers;
		while(q) {
			killArgs.pid = q->p_pid;
			/*
		         * The interface for kill is better
			 * than the internal signal
			 */
			kill(p, &killArgs);
			nq = q;
			q = q->p_peers;
		}
		while (p->p_peers) 
		  tsleep((caddr_t)p, PWAIT, "exit1", 0);
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

	if (p->p_flag & P_PROFIL)
		stopprofclock(p);
	MALLOC(p->p_ru, struct rusage *, sizeof(struct rusage),
		M_ZOMBIE, M_WAITOK);
	/*
	 * If parent is waiting for us to exit or exec,
	 * P_PPWAIT is set; we will wakeup the parent below.
	 */
	p->p_flag &= ~(P_TRACED | P_PPWAIT);
	p->p_flag |= P_WEXIT;
	SIGEMPTYSET(p->p_siglist);
	if (timevalisset(&p->p_realtimer.it_value))
		untimeout(realitexpire, (caddr_t)p, p->p_ithandle);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pid.
	 */
	funsetownlst(&p->p_sigiolst);

	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
	fdfree(p);

	if(p->p_leader->p_peers) {
		q = p->p_leader;
		while(q->p_peers != p)
			q = q->p_peers;
		q->p_peers = p->p_peers;
		wakeup((caddr_t)p->p_leader);
	}

	/*
	 * XXX Shutdown SYSV semaphores
	 */
	semexit(p);

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

	if (SESS_LEADER(p)) {
		register struct session *sp = p->p_session;

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
	}
	fixjobc(p, p->p_pgrp, 0);
	(void)acct_process(p);
#ifdef KTRACE
	/*
	 * release trace file
	 */
	p->p_traceflag = 0;	/* don't trace the vrele() */
	if (p->p_tracep)
		vrele(p->p_tracep);
#endif
	/*
	 * Remove proc from allproc queue and pidhash chain.
	 * Place onto zombproc.  Unlink from parent's child list.
	 */
	LIST_REMOVE(p, p_list);
	LIST_INSERT_HEAD(&zombproc, p, p_list);
	p->p_stat = SZOMB;

	LIST_REMOVE(p, p_hash);

	q = LIST_FIRST(&p->p_children);
	if (q)		/* only need this if any child is S_ZOMB */
		wakeup((caddr_t) initproc);
	for (; q != 0; q = nq) {
		nq = LIST_NEXT(q, p_sibling);
		LIST_REMOVE(q, p_sibling);
		LIST_INSERT_HEAD(&initproc->p_children, q, p_sibling);
		q->p_pptr = initproc;
		q->p_sigparent = SIGCHLD;
		/*
		 * Traced processes are killed
		 * since their existence means someone is screwing up.
		 */
		if (q->p_flag & P_TRACED) {
			q->p_flag &= ~P_TRACED;
			psignal(q, SIGKILL);
		}
	}

	/*
	 * Save exit status and final rusage info, adding in child rusage
	 * info and self times.
	 */
	p->p_xstat = rv;
	*p->p_ru = p->p_stats->p_ru;
	calcru(p, &p->p_ru->ru_utime, &p->p_ru->ru_stime, NULL);
	ruadd(p->p_ru, &p->p_stats->p_cru);

	/*
	 * Pretend that an mi_switch() to the next process occurs now.  We
	 * must set `switchtime' directly since we will call cpu_switch()
	 * directly.  Set it now so that the rest of the exit time gets
	 * counted somewhere if possible.
	 */
	microuptime(&switchtime);
	switchticks = ticks;

	/*
	 * notify interested parties of our demise.
	 */
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

	if (p->p_sigparent && p->p_pptr != initproc) {
	        psignal(p->p_pptr, p->p_sigparent);
	} else {
	        psignal(p->p_pptr, SIGCHLD);
	}

	wakeup((caddr_t)p->p_pptr);
#if defined(tahoe)
	/* move this to cpu_exit */
	p->p_addr->u_pcb.pcb_savacc.faddr = (float *)NULL;
#endif
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
	SET_CURPROC(NULL);
	if (--p->p_limit->p_refcnt == 0) {
		FREE(p->p_limit, M_SUBPROC);
		p->p_limit = NULL;
	}

	/*
	 * Finally, call machine-dependent code to release the remaining
	 * resources including address space, the kernel stack and pcb.
	 * The address space is released by "vmspace_free(p->p_vmspace)";
	 * This is machine-dependent, as we may have to change stacks
	 * or ensure that the current one isn't reallocated before we
	 * finish.  cpu_exit will end with a call to cpu_switch(), finishing
	 * our execution (pun intended).
	 */
	cpu_exit(p);
}

#ifdef COMPAT_43
int
owait(p, uap)
	struct proc *p;
	register struct owait_args /* {
		int     dummy;
	} */ *uap;
{
	struct wait_args w;

	w.options = 0;
	w.rusage = NULL;
	w.pid = WAIT_ANY;
	w.status = NULL;
	return (wait1(p, &w, 1));
}
#endif /* COMPAT_43 */

int
wait4(p, uap)
	struct proc *p;
	struct wait_args *uap;
{

	return (wait1(p, uap, 0));
}

static int
wait1(q, uap, compat)
	register struct proc *q;
	register struct wait_args /* {
		int pid;
		int *status;
		int options;
		struct rusage *rusage;
	} */ *uap;
	int compat;
{
	register int nfound;
	register struct proc *p, *t;
	int status, error;

	if (uap->pid == 0)
		uap->pid = -q->p_pgid;
	if (uap->options &~ (WUNTRACED|WNOHANG|WLINUXCLONE))
		return (EINVAL);
loop:
	nfound = 0;
	LIST_FOREACH(p, &q->p_children, p_sibling) {
		if (uap->pid != WAIT_ANY &&
		    p->p_pid != uap->pid && p->p_pgid != -uap->pid)
			continue;

		/* This special case handles a kthread spawned by linux_clone 
		 * (see linux_misc.c).  The linux_wait4 and linux_waitpid functions
		 * need to be able to distinguish between waiting on a process and
		 * waiting on a thread.  It is a thread if p_sigparent is not SIGCHLD,
		 * and the WLINUXCLONE option signifies we want to wait for threads
		 * and not processes.
		 */
		if ((p->p_sigparent != SIGCHLD) ^ ((uap->options & WLINUXCLONE) != 0))
			continue;

		nfound++;
		if (p->p_stat == SZOMB) {
			/* charge childs scheduling cpu usage to parent */
			if (curproc->p_pid != 1) {
				curproc->p_estcpu =
				    ESTCPULIM(curproc->p_estcpu + p->p_estcpu);
			}

			q->p_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat)
				q->p_retval[1] = p->p_xstat;
			else
#endif
			if (uap->status) {
				status = p->p_xstat;	/* convert to int */
				if ((error = copyout((caddr_t)&status,
				    (caddr_t)uap->status, sizeof(status))))
					return (error);
			}
			if (uap->rusage && (error = copyout((caddr_t)p->p_ru,
			    (caddr_t)uap->rusage, sizeof (struct rusage))))
				return (error);
			/*
			 * If we got the child via a ptrace 'attach',
			 * we need to give it back to the old parent.
			 */
			if (p->p_oppid && (t = pfind(p->p_oppid))) {
				p->p_oppid = 0;
				proc_reparent(p, t);
				psignal(t, SIGCHLD);
				wakeup((caddr_t)t);
				return (0);
			}
			p->p_xstat = 0;
			ruadd(&q->p_stats->p_cru, p->p_ru);
			FREE(p->p_ru, M_ZOMBIE);
			p->p_ru = NULL;

			/*
			 * Decrement the count of procs running with this uid.
			 */
			(void)chgproccnt(p->p_cred->p_uidinfo, -1, 0);

			/*
			 * Release reference to text vnode
			 */
			if (p->p_textvp)
				vrele(p->p_textvp);

			/*
			 * Free up credentials.
			 */
			if (--p->p_cred->p_refcnt == 0) {
				crfree(p->p_ucred);
				uifree(p->p_cred->p_uidinfo);
				FREE(p->p_cred, M_SUBPROC);
				p->p_cred = NULL;
			}

			/*
			 * Destroy empty prisons
			 */
			if (p->p_prison && !--p->p_prison->pr_ref) {
				if (p->p_prison->pr_linux != NULL)
					FREE(p->p_prison->pr_linux, M_PRISON);
				FREE(p->p_prison, M_PRISON);
			}

			/*
			 * Remove unused arguments
			 */
			if (p->p_args && --p->p_args->ar_ref == 0)
				FREE(p->p_args, M_PARGS);

			/*
			 * Finally finished with old proc entry.
			 * Unlink it from its process group and free it.
			 */
			leavepgrp(p);
			LIST_REMOVE(p, p_list);	/* off zombproc */
			LIST_REMOVE(p, p_sibling);

			if (--p->p_procsig->ps_refcnt == 0) {
				if (p->p_sigacts != &p->p_addr->u_sigacts)
					FREE(p->p_sigacts, M_SUBPROC);
			        FREE(p->p_procsig, M_SUBPROC);
				p->p_procsig = NULL;
			}

			/*
			 * Give machine-dependent layer a chance
			 * to free anything that cpu_exit couldn't
			 * release while still running in process context.
			 */
			cpu_wait(p);
			zfree(proc_zone, p);
			nprocs--;
			return (0);
		}
		if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED || uap->options & WUNTRACED)) {
			p->p_flag |= P_WAITED;
			q->p_retval[0] = p->p_pid;
#ifdef COMPAT_43
			if (compat) {
				q->p_retval[1] = W_STOPCODE(p->p_xstat);
				error = 0;
			} else
#endif
			if (uap->status) {
				status = W_STOPCODE(p->p_xstat);
				error = copyout((caddr_t)&status,
					(caddr_t)uap->status, sizeof(status));
			} else
				error = 0;
			return (error);
		}
	}
	if (nfound == 0)
		return (ECHILD);
	if (uap->options & WNOHANG) {
		q->p_retval[0] = 0;
		return (0);
	}
	if ((error = tsleep((caddr_t)q, PWAIT | PCATCH, "wait", 0)))
		return (error);
	goto loop;
}

/*
 * make process 'parent' the new parent of process 'child'.
 */
void
proc_reparent(child, parent)
	register struct proc *child;
	register struct proc *parent;
{

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

void check_sigacts (void)
{
	struct proc *p = curproc;
	struct sigacts *pss;
	int s;

	if (p->p_procsig->ps_refcnt == 1 &&
	    p->p_sigacts != &p->p_addr->u_sigacts) {
		pss = p->p_sigacts;
		s = splhigh();
		p->p_addr->u_sigacts = *pss;
		p->p_sigacts = &p->p_addr->u_sigacts;
		splx(s);
		FREE(pss, M_SUBPROC);
	}
}

