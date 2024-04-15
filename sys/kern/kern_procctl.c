/*-
 * Copyright (c) 2014 John Baldwin
 * Copyright (c) 2014, 2016 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/_unrhdr.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/procctl.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/taskqueue.h>
#include <sys/wait.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

static int
protect_setchild(struct thread *td, struct proc *p, int flags)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (p->p_flag & P_SYSTEM || p_cansched(td, p) != 0)
		return (0);
	if (flags & PPROT_SET) {
		p->p_flag |= P_PROTECTED;
		if (flags & PPROT_INHERIT)
			p->p_flag2 |= P2_INHERIT_PROTECTED;
	} else {
		p->p_flag &= ~P_PROTECTED;
		p->p_flag2 &= ~P2_INHERIT_PROTECTED;
	}
	return (1);
}

static int
protect_setchildren(struct thread *td, struct proc *top, int flags)
{
	struct proc *p;
	int ret;

	p = top;
	ret = 0;
	sx_assert(&proctree_lock, SX_LOCKED);
	for (;;) {
		ret |= protect_setchild(td, p, flags);
		PROC_UNLOCK(p);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top) {
				PROC_LOCK(p);
				return (ret);
			}
			if (LIST_NEXT(p, p_sibling)) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
		PROC_LOCK(p);
	}
}

static int
protect_set(struct thread *td, struct proc *p, void *data)
{
	int error, flags, ret;

	flags = *(int *)data;
	switch (PPROT_OP(flags)) {
	case PPROT_SET:
	case PPROT_CLEAR:
		break;
	default:
		return (EINVAL);
	}

	if ((PPROT_FLAGS(flags) & ~(PPROT_DESCEND | PPROT_INHERIT)) != 0)
		return (EINVAL);

	error = priv_check(td, PRIV_VM_MADV_PROTECT);
	if (error)
		return (error);

	if (flags & PPROT_DESCEND)
		ret = protect_setchildren(td, p, flags);
	else
		ret = protect_setchild(td, p, flags);
	if (ret == 0)
		return (EPERM);
	return (0);
}

static int
reap_acquire(struct thread *td, struct proc *p, void *data __unused)
{

	sx_assert(&proctree_lock, SX_XLOCKED);
	if (p != td->td_proc)
		return (EPERM);
	if ((p->p_treeflag & P_TREE_REAPER) != 0)
		return (EBUSY);
	p->p_treeflag |= P_TREE_REAPER;
	/*
	 * We do not reattach existing children and the whole tree
	 * under them to us, since p->p_reaper already seen them.
	 */
	return (0);
}

static int
reap_release(struct thread *td, struct proc *p, void *data __unused)
{

	sx_assert(&proctree_lock, SX_XLOCKED);
	if (p != td->td_proc)
		return (EPERM);
	if (p == initproc)
		return (EINVAL);
	if ((p->p_treeflag & P_TREE_REAPER) == 0)
		return (EINVAL);
	reaper_abandon_children(p, false);
	return (0);
}

static int
reap_status(struct thread *td, struct proc *p, void *data)
{
	struct proc *reap, *p2, *first_p;
	struct procctl_reaper_status *rs;

	rs = data;
	sx_assert(&proctree_lock, SX_LOCKED);
	if ((p->p_treeflag & P_TREE_REAPER) == 0) {
		reap = p->p_reaper;
	} else {
		reap = p;
		rs->rs_flags |= REAPER_STATUS_OWNED;
	}
	if (reap == initproc)
		rs->rs_flags |= REAPER_STATUS_REALINIT;
	rs->rs_reaper = reap->p_pid;
	rs->rs_descendants = 0;
	rs->rs_children = 0;
	if (!LIST_EMPTY(&reap->p_reaplist)) {
		first_p = LIST_FIRST(&reap->p_children);
		if (first_p == NULL)
			first_p = LIST_FIRST(&reap->p_reaplist);
		rs->rs_pid = first_p->p_pid;
		LIST_FOREACH(p2, &reap->p_reaplist, p_reapsibling) {
			if (proc_realparent(p2) == reap)
				rs->rs_children++;
			rs->rs_descendants++;
		}
	} else {
		rs->rs_pid = -1;
	}
	return (0);
}

static int
reap_getpids(struct thread *td, struct proc *p, void *data)
{
	struct proc *reap, *p2;
	struct procctl_reaper_pidinfo *pi, *pip;
	struct procctl_reaper_pids *rp;
	u_int i, n;
	int error;

	rp = data;
	sx_assert(&proctree_lock, SX_LOCKED);
	PROC_UNLOCK(p);
	reap = (p->p_treeflag & P_TREE_REAPER) == 0 ? p->p_reaper : p;
	n = i = 0;
	error = 0;
	LIST_FOREACH(p2, &reap->p_reaplist, p_reapsibling)
		n++;
	sx_unlock(&proctree_lock);
	if (rp->rp_count < n)
		n = rp->rp_count;
	pi = malloc(n * sizeof(*pi), M_TEMP, M_WAITOK);
	sx_slock(&proctree_lock);
	LIST_FOREACH(p2, &reap->p_reaplist, p_reapsibling) {
		if (i == n)
			break;
		pip = &pi[i];
		bzero(pip, sizeof(*pip));
		pip->pi_pid = p2->p_pid;
		pip->pi_subtree = p2->p_reapsubtree;
		pip->pi_flags = REAPER_PIDINFO_VALID;
		if (proc_realparent(p2) == reap)
			pip->pi_flags |= REAPER_PIDINFO_CHILD;
		if ((p2->p_treeflag & P_TREE_REAPER) != 0)
			pip->pi_flags |= REAPER_PIDINFO_REAPER;
		if ((p2->p_flag & P_STOPPED) != 0)
			pip->pi_flags |= REAPER_PIDINFO_STOPPED;
		if (p2->p_state == PRS_ZOMBIE)
			pip->pi_flags |= REAPER_PIDINFO_ZOMBIE;
		else if ((p2->p_flag & P_WEXIT) != 0)
			pip->pi_flags |= REAPER_PIDINFO_EXITING;
		i++;
	}
	sx_sunlock(&proctree_lock);
	error = copyout(pi, rp->rp_pids, i * sizeof(*pi));
	free(pi, M_TEMP);
	sx_slock(&proctree_lock);
	PROC_LOCK(p);
	return (error);
}

struct reap_kill_proc_work {
	struct ucred *cr;
	struct proc *target;
	ksiginfo_t *ksi;
	struct procctl_reaper_kill *rk;
	int *error;
	struct task t;
};

static void
reap_kill_proc_locked(struct reap_kill_proc_work *w)
{
	int error1;
	bool need_stop;

	PROC_LOCK_ASSERT(w->target, MA_OWNED);
	PROC_ASSERT_HELD(w->target);

	error1 = cr_cansignal(w->cr, w->target, w->rk->rk_sig);
	if (error1 != 0) {
		if (*w->error == ESRCH) {
			w->rk->rk_fpid = w->target->p_pid;
			*w->error = error1;
		}
		return;
	}

	/*
	 * The need_stop indicates if the target process needs to be
	 * suspended before being signalled.  This is needed when we
	 * guarantee that all processes in subtree are signalled,
	 * avoiding the race with some process not yet fully linked
	 * into all structures during fork, ignored by iterator, and
	 * then escaping signalling.
	 *
	 * The thread cannot usefully stop itself anyway, and if other
	 * thread of the current process forks while the current
	 * thread signals the whole subtree, it is an application
	 * race.
	 */
	if ((w->target->p_flag & (P_KPROC | P_SYSTEM | P_STOPPED)) == 0)
		need_stop = thread_single(w->target, SINGLE_ALLPROC) == 0;
	else
		need_stop = false;

	(void)pksignal(w->target, w->rk->rk_sig, w->ksi);
	w->rk->rk_killed++;
	*w->error = error1;

	if (need_stop)
		thread_single_end(w->target, SINGLE_ALLPROC);
}

static void
reap_kill_proc_work(void *arg, int pending __unused)
{
	struct reap_kill_proc_work *w;

	w = arg;
	PROC_LOCK(w->target);
	if ((w->target->p_flag2 & P2_WEXIT) == 0)
		reap_kill_proc_locked(w);
	PROC_UNLOCK(w->target);

	sx_xlock(&proctree_lock);
	w->target = NULL;
	wakeup(&w->target);
	sx_xunlock(&proctree_lock);
}

struct reap_kill_tracker {
	struct proc *parent;
	TAILQ_ENTRY(reap_kill_tracker) link;
};

TAILQ_HEAD(reap_kill_tracker_head, reap_kill_tracker);

static void
reap_kill_sched(struct reap_kill_tracker_head *tracker, struct proc *p2)
{
	struct reap_kill_tracker *t;

	PROC_LOCK(p2);
	if ((p2->p_flag2 & P2_WEXIT) != 0) {
		PROC_UNLOCK(p2);
		return;
	}
	_PHOLD_LITE(p2);
	PROC_UNLOCK(p2);
	t = malloc(sizeof(struct reap_kill_tracker), M_TEMP, M_WAITOK);
	t->parent = p2;
	TAILQ_INSERT_TAIL(tracker, t, link);
}

static void
reap_kill_sched_free(struct reap_kill_tracker *t)
{
	PRELE(t->parent);
	free(t, M_TEMP);
}

static void
reap_kill_children(struct thread *td, struct proc *reaper,
    struct procctl_reaper_kill *rk, ksiginfo_t *ksi, int *error)
{
	struct proc *p2;
	int error1;

	LIST_FOREACH(p2, &reaper->p_children, p_sibling) {
		PROC_LOCK(p2);
		if ((p2->p_flag2 & P2_WEXIT) == 0) {
			error1 = p_cansignal(td, p2, rk->rk_sig);
			if (error1 != 0) {
				if (*error == ESRCH) {
					rk->rk_fpid = p2->p_pid;
					*error = error1;
				}

				/*
				 * Do not end the loop on error,
				 * signal everything we can.
				 */
			} else {
				(void)pksignal(p2, rk->rk_sig, ksi);
				rk->rk_killed++;
			}
		}
		PROC_UNLOCK(p2);
	}
}

static bool
reap_kill_subtree_once(struct thread *td, struct proc *p, struct proc *reaper,
    struct unrhdr *pids, struct reap_kill_proc_work *w)
{
	struct reap_kill_tracker_head tracker;
	struct reap_kill_tracker *t;
	struct proc *p2;
	int r, xlocked;
	bool res, st;

	res = false;
	TAILQ_INIT(&tracker);
	reap_kill_sched(&tracker, reaper);
	while ((t = TAILQ_FIRST(&tracker)) != NULL) {
		TAILQ_REMOVE(&tracker, t, link);

		/*
		 * Since reap_kill_proc() drops proctree_lock sx, it
		 * is possible that the tracked reaper is no longer.
		 * In this case the subtree is reparented to the new
		 * reaper, which should handle it.
		 */
		if ((t->parent->p_treeflag & P_TREE_REAPER) == 0) {
			reap_kill_sched_free(t);
			res = true;
			continue;
		}

		LIST_FOREACH(p2, &t->parent->p_reaplist, p_reapsibling) {
			if (t->parent == reaper &&
			    (w->rk->rk_flags & REAPER_KILL_SUBTREE) != 0 &&
			    p2->p_reapsubtree != w->rk->rk_subtree)
				continue;
			if ((p2->p_treeflag & P_TREE_REAPER) != 0)
				reap_kill_sched(&tracker, p2);

			/*
			 * Handle possible pid reuse.  If we recorded
			 * p2 as killed but its p_flag2 does not
			 * confirm it, that means that the process
			 * terminated and its id was reused by other
			 * process in the reaper subtree.
			 *
			 * Unlocked read of p2->p_flag2 is fine, it is
			 * our thread that set the tested flag.
			 */
			if (alloc_unr_specific(pids, p2->p_pid) != p2->p_pid &&
			    (atomic_load_int(&p2->p_flag2) &
			    (P2_REAPKILLED | P2_WEXIT)) != 0)
				continue;

			if (p2 == td->td_proc) {
				if ((p2->p_flag & P_HADTHREADS) != 0 &&
				    (p2->p_flag2 & P2_WEXIT) == 0) {
					xlocked = sx_xlocked(&proctree_lock);
					sx_unlock(&proctree_lock);
					st = true;
				} else {
					st = false;
				}
				PROC_LOCK(p2);
				/*
				 * sapblk ensures that only one thread
				 * in the system sets this flag.
				 */
				p2->p_flag2 |= P2_REAPKILLED;
				if (st)
					r = thread_single(p2, SINGLE_NO_EXIT);
				(void)pksignal(p2, w->rk->rk_sig, w->ksi);
				w->rk->rk_killed++;
				if (st && r == 0)
					thread_single_end(p2, SINGLE_NO_EXIT);
				PROC_UNLOCK(p2);
				if (st) {
					if (xlocked)
						sx_xlock(&proctree_lock);
					else
						sx_slock(&proctree_lock);
				}
			} else {
				PROC_LOCK(p2);
				if ((p2->p_flag2 & P2_WEXIT) == 0) {
					_PHOLD_LITE(p2);
					p2->p_flag2 |= P2_REAPKILLED;
					PROC_UNLOCK(p2);
					w->target = p2;
					taskqueue_enqueue(taskqueue_thread,
					    &w->t);
					while (w->target != NULL) {
						sx_sleep(&w->target,
						    &proctree_lock, PWAIT,
						    "reapst", 0);
					}
					PROC_LOCK(p2);
					_PRELE(p2);
				}
				PROC_UNLOCK(p2);
			}
			res = true;
		}
		reap_kill_sched_free(t);
	}
	return (res);
}

static void
reap_kill_subtree(struct thread *td, struct proc *p, struct proc *reaper,
    struct reap_kill_proc_work *w)
{
	struct unrhdr pids;
	void *ihandle;
	struct proc *p2;
	int pid;

	/*
	 * pids records processes which were already signalled, to
	 * avoid doubling signals to them if iteration needs to be
	 * repeated.
	 */
	init_unrhdr(&pids, 1, PID_MAX, UNR_NO_MTX);
	PROC_LOCK(td->td_proc);
	if ((td->td_proc->p_flag2 & P2_WEXIT) != 0) {
		PROC_UNLOCK(td->td_proc);
		goto out;
	}
	PROC_UNLOCK(td->td_proc);
	while (reap_kill_subtree_once(td, p, reaper, &pids, w))
	       ;

	ihandle = create_iter_unr(&pids);
	while ((pid = next_iter_unr(ihandle)) != -1) {
		p2 = pfind(pid);
		if (p2 != NULL) {
			p2->p_flag2 &= ~P2_REAPKILLED;
			PROC_UNLOCK(p2);
		}
	}
	free_iter_unr(ihandle);

out:
	clean_unrhdr(&pids);
	clear_unrhdr(&pids);
}

static bool
reap_kill_sapblk(struct thread *td __unused, void *data)
{
	struct procctl_reaper_kill *rk;

	rk = data;
	return ((rk->rk_flags & REAPER_KILL_CHILDREN) == 0);
}

static int
reap_kill(struct thread *td, struct proc *p, void *data)
{
	struct reap_kill_proc_work w;
	struct proc *reaper;
	ksiginfo_t ksi;
	struct procctl_reaper_kill *rk;
	int error;

	rk = data;
	sx_assert(&proctree_lock, SX_LOCKED);
	if (CAP_TRACING(td))
		ktrcapfail(CAPFAIL_SIGNAL, &rk->rk_sig);
	if (IN_CAPABILITY_MODE(td))
		return (ECAPMODE);
	if (rk->rk_sig <= 0 || rk->rk_sig > _SIG_MAXSIG ||
	    (rk->rk_flags & ~(REAPER_KILL_CHILDREN |
	    REAPER_KILL_SUBTREE)) != 0 || (rk->rk_flags &
	    (REAPER_KILL_CHILDREN | REAPER_KILL_SUBTREE)) ==
	    (REAPER_KILL_CHILDREN | REAPER_KILL_SUBTREE))
		return (EINVAL);
	PROC_UNLOCK(p);
	reaper = (p->p_treeflag & P_TREE_REAPER) == 0 ? p->p_reaper : p;
	ksiginfo_init(&ksi);
	ksi.ksi_signo = rk->rk_sig;
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_ucred->cr_ruid;
	error = ESRCH;
	rk->rk_killed = 0;
	rk->rk_fpid = -1;
	if ((rk->rk_flags & REAPER_KILL_CHILDREN) != 0) {
		reap_kill_children(td, reaper, rk, &ksi, &error);
	} else {
		w.cr = crhold(td->td_ucred);
		w.ksi = &ksi;
		w.rk = rk;
		w.error = &error;
		TASK_INIT(&w.t, 0, reap_kill_proc_work, &w);

		/*
		 * Prevent swapout, since w, ksi, and possibly rk, are
		 * allocated on the stack.  We sleep in
		 * reap_kill_subtree_once() waiting for task to
		 * complete single-threading.
		 */
		PHOLD(td->td_proc);

		reap_kill_subtree(td, p, reaper, &w);
		PRELE(td->td_proc);
		crfree(w.cr);
	}
	PROC_LOCK(p);
	return (error);
}

static int
trace_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	/*
	 * Ktrace changes p_traceflag from or to zero under the
	 * process lock, so the test does not need to acquire ktrace
	 * mutex.
	 */
	if ((p->p_flag & P_TRACED) != 0 || p->p_traceflag != 0)
		return (EBUSY);

	switch (state) {
	case PROC_TRACE_CTL_ENABLE:
		if (td->td_proc != p)
			return (EPERM);
		p->p_flag2 &= ~(P2_NOTRACE | P2_NOTRACE_EXEC);
		break;
	case PROC_TRACE_CTL_DISABLE_EXEC:
		p->p_flag2 |= P2_NOTRACE_EXEC | P2_NOTRACE;
		break;
	case PROC_TRACE_CTL_DISABLE:
		if ((p->p_flag2 & P2_NOTRACE_EXEC) != 0) {
			KASSERT((p->p_flag2 & P2_NOTRACE) != 0,
			    ("dandling P2_NOTRACE_EXEC"));
			if (td->td_proc != p)
				return (EPERM);
			p->p_flag2 &= ~P2_NOTRACE_EXEC;
		} else {
			p->p_flag2 |= P2_NOTRACE;
		}
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
trace_status(struct thread *td, struct proc *p, void *data)
{
	int *status;

	status = data;
	if ((p->p_flag2 & P2_NOTRACE) != 0) {
		KASSERT((p->p_flag & P_TRACED) == 0,
		    ("%d traced but tracing disabled", p->p_pid));
		*status = -1;
	} else if ((p->p_flag & P_TRACED) != 0) {
		*status = p->p_pptr->p_pid;
	} else {
		*status = 0;
	}
	return (0);
}

static int
trapcap_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	switch (state) {
	case PROC_TRAPCAP_CTL_ENABLE:
		p->p_flag2 |= P2_TRAPCAP;
		break;
	case PROC_TRAPCAP_CTL_DISABLE:
		p->p_flag2 &= ~P2_TRAPCAP;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
trapcap_status(struct thread *td, struct proc *p, void *data)
{
	int *status;

	status = data;
	*status = (p->p_flag2 & P2_TRAPCAP) != 0 ? PROC_TRAPCAP_CTL_ENABLE :
	    PROC_TRAPCAP_CTL_DISABLE;
	return (0);
}

static int
no_new_privs_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	if (state != PROC_NO_NEW_PRIVS_ENABLE)
		return (EINVAL);
	p->p_flag2 |= P2_NO_NEW_PRIVS;
	return (0);
}

static int
no_new_privs_status(struct thread *td, struct proc *p, void *data)
{

	*(int *)data = (p->p_flag2 & P2_NO_NEW_PRIVS) != 0 ?
	    PROC_NO_NEW_PRIVS_ENABLE : PROC_NO_NEW_PRIVS_DISABLE;
	return (0);
}

static int
protmax_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	switch (state) {
	case PROC_PROTMAX_FORCE_ENABLE:
		p->p_flag2 &= ~P2_PROTMAX_DISABLE;
		p->p_flag2 |= P2_PROTMAX_ENABLE;
		break;
	case PROC_PROTMAX_FORCE_DISABLE:
		p->p_flag2 |= P2_PROTMAX_DISABLE;
		p->p_flag2 &= ~P2_PROTMAX_ENABLE;
		break;
	case PROC_PROTMAX_NOFORCE:
		p->p_flag2 &= ~(P2_PROTMAX_ENABLE | P2_PROTMAX_DISABLE);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
protmax_status(struct thread *td, struct proc *p, void *data)
{
	int d;

	switch (p->p_flag2 & (P2_PROTMAX_ENABLE | P2_PROTMAX_DISABLE)) {
	case 0:
		d = PROC_PROTMAX_NOFORCE;
		break;
	case P2_PROTMAX_ENABLE:
		d = PROC_PROTMAX_FORCE_ENABLE;
		break;
	case P2_PROTMAX_DISABLE:
		d = PROC_PROTMAX_FORCE_DISABLE;
		break;
	}
	if (kern_mmap_maxprot(p, PROT_READ) == PROT_READ)
		d |= PROC_PROTMAX_ACTIVE;
	*(int *)data = d;
	return (0);
}

static int
aslr_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	switch (state) {
	case PROC_ASLR_FORCE_ENABLE:
		p->p_flag2 &= ~P2_ASLR_DISABLE;
		p->p_flag2 |= P2_ASLR_ENABLE;
		break;
	case PROC_ASLR_FORCE_DISABLE:
		p->p_flag2 |= P2_ASLR_DISABLE;
		p->p_flag2 &= ~P2_ASLR_ENABLE;
		break;
	case PROC_ASLR_NOFORCE:
		p->p_flag2 &= ~(P2_ASLR_ENABLE | P2_ASLR_DISABLE);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
aslr_status(struct thread *td, struct proc *p, void *data)
{
	struct vmspace *vm;
	int d;

	switch (p->p_flag2 & (P2_ASLR_ENABLE | P2_ASLR_DISABLE)) {
	case 0:
		d = PROC_ASLR_NOFORCE;
		break;
	case P2_ASLR_ENABLE:
		d = PROC_ASLR_FORCE_ENABLE;
		break;
	case P2_ASLR_DISABLE:
		d = PROC_ASLR_FORCE_DISABLE;
		break;
	}
	if ((p->p_flag & P_WEXIT) == 0) {
		_PHOLD(p);
		PROC_UNLOCK(p);
		vm = vmspace_acquire_ref(p);
		if (vm != NULL) {
			if ((vm->vm_map.flags & MAP_ASLR) != 0)
				d |= PROC_ASLR_ACTIVE;
			vmspace_free(vm);
		}
		PROC_LOCK(p);
		_PRELE(p);
	}
	*(int *)data = d;
	return (0);
}

static int
stackgap_ctl(struct thread *td, struct proc *p, void *data)
{
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	state = *(int *)data;

	if ((state & ~(PROC_STACKGAP_ENABLE | PROC_STACKGAP_DISABLE |
	    PROC_STACKGAP_ENABLE_EXEC | PROC_STACKGAP_DISABLE_EXEC)) != 0)
		return (EINVAL);
	switch (state & (PROC_STACKGAP_ENABLE | PROC_STACKGAP_DISABLE)) {
	case PROC_STACKGAP_ENABLE:
		if ((p->p_flag2 & P2_STKGAP_DISABLE) != 0)
			return (EINVAL);
		break;
	case PROC_STACKGAP_DISABLE:
		p->p_flag2 |= P2_STKGAP_DISABLE;
		break;
	case 0:
		break;
	default:
		return (EINVAL);
	}
	switch (state & (PROC_STACKGAP_ENABLE_EXEC |
	    PROC_STACKGAP_DISABLE_EXEC)) {
	case PROC_STACKGAP_ENABLE_EXEC:
		p->p_flag2 &= ~P2_STKGAP_DISABLE_EXEC;
		break;
	case PROC_STACKGAP_DISABLE_EXEC:
		p->p_flag2 |= P2_STKGAP_DISABLE_EXEC;
		break;
	case 0:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
stackgap_status(struct thread *td, struct proc *p, void *data)
{
	int d;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	d = (p->p_flag2 & P2_STKGAP_DISABLE) != 0 ? PROC_STACKGAP_DISABLE :
	    PROC_STACKGAP_ENABLE;
	d |= (p->p_flag2 & P2_STKGAP_DISABLE_EXEC) != 0 ?
	    PROC_STACKGAP_DISABLE_EXEC : PROC_STACKGAP_ENABLE_EXEC;
	*(int *)data = d;
	return (0);
}

static int
wxmap_ctl(struct thread *td, struct proc *p, void *data)
{
	struct vmspace *vm;
	vm_map_t map;
	int state;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((p->p_flag & P_WEXIT) != 0)
		return (ESRCH);
	state = *(int *)data;

	switch (state) {
	case PROC_WX_MAPPINGS_PERMIT:
		p->p_flag2 |= P2_WXORX_DISABLE;
		_PHOLD(p);
		PROC_UNLOCK(p);
		vm = vmspace_acquire_ref(p);
		if (vm != NULL) {
			map = &vm->vm_map;
			vm_map_lock(map);
			map->flags &= ~MAP_WXORX;
			vm_map_unlock(map);
			vmspace_free(vm);
		}
		PROC_LOCK(p);
		_PRELE(p);
		break;
	case PROC_WX_MAPPINGS_DISALLOW_EXEC:
		p->p_flag2 |= P2_WXORX_ENABLE_EXEC;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
wxmap_status(struct thread *td, struct proc *p, void *data)
{
	struct vmspace *vm;
	int d;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((p->p_flag & P_WEXIT) != 0)
		return (ESRCH);

	d = 0;
	if ((p->p_flag2 & P2_WXORX_DISABLE) != 0)
		d |= PROC_WX_MAPPINGS_PERMIT;
	if ((p->p_flag2 & P2_WXORX_ENABLE_EXEC) != 0)
		d |= PROC_WX_MAPPINGS_DISALLOW_EXEC;
	_PHOLD(p);
	PROC_UNLOCK(p);
	vm = vmspace_acquire_ref(p);
	if (vm != NULL) {
		if ((vm->vm_map.flags & MAP_WXORX) != 0)
			d |= PROC_WXORX_ENFORCE;
		vmspace_free(vm);
	}
	PROC_LOCK(p);
	_PRELE(p);
	*(int *)data = d;
	return (0);
}

static int
pdeathsig_ctl(struct thread *td, struct proc *p, void *data)
{
	int signum;

	signum = *(int *)data;
	if (p != td->td_proc || (signum != 0 && !_SIG_VALID(signum)))
		return (EINVAL);
	p->p_pdeathsig = signum;
	return (0);
}

static int
pdeathsig_status(struct thread *td, struct proc *p, void *data)
{
	if (p != td->td_proc)
		return (EINVAL);
	*(int *)data = p->p_pdeathsig;
	return (0);
}

enum {
	PCTL_SLOCKED,
	PCTL_XLOCKED,
	PCTL_UNLOCKED,
};

struct procctl_cmd_info {
	int lock_tree;
	bool one_proc : 1;
	bool esrch_is_einval : 1;
	bool copyout_on_error : 1;
	bool no_nonnull_data : 1;
	bool need_candebug : 1;
	int copyin_sz;
	int copyout_sz;
	int (*exec)(struct thread *, struct proc *, void *);
	bool (*sapblk)(struct thread *, void *);
};
static const struct procctl_cmd_info procctl_cmds_info[] = {
	[PROC_SPROTECT] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = false,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = protect_set, .copyout_on_error = false, },
	[PROC_REAP_ACQUIRE] =
	    { .lock_tree = PCTL_XLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = true,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = 0,
	      .exec = reap_acquire, .copyout_on_error = false, },
	[PROC_REAP_RELEASE] =
	    { .lock_tree = PCTL_XLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = true,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = 0,
	      .exec = reap_release, .copyout_on_error = false, },
	[PROC_REAP_STATUS] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0,
	      .copyout_sz = sizeof(struct procctl_reaper_status),
	      .exec = reap_status, .copyout_on_error = false, },
	[PROC_REAP_GETPIDS] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = sizeof(struct procctl_reaper_pids),
	      .copyout_sz = 0,
	      .exec = reap_getpids, .copyout_on_error = false, },
	[PROC_REAP_KILL] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = sizeof(struct procctl_reaper_kill),
	      .copyout_sz = sizeof(struct procctl_reaper_kill),
	      .exec = reap_kill, .copyout_on_error = true,
	      .sapblk = reap_kill_sapblk, },
	[PROC_TRACE_CTL] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = false,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = trace_ctl, .copyout_on_error = false, },
	[PROC_TRACE_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = trace_status, .copyout_on_error = false, },
	[PROC_TRAPCAP_CTL] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = false,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = trapcap_ctl, .copyout_on_error = false, },
	[PROC_TRAPCAP_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = trapcap_status, .copyout_on_error = false, },
	[PROC_PDEATHSIG_CTL] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = true, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = pdeathsig_ctl, .copyout_on_error = false, },
	[PROC_PDEATHSIG_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = true, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = pdeathsig_status, .copyout_on_error = false, },
	[PROC_ASLR_CTL] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = aslr_ctl, .copyout_on_error = false, },
	[PROC_ASLR_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = aslr_status, .copyout_on_error = false, },
	[PROC_PROTMAX_CTL] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = protmax_ctl, .copyout_on_error = false, },
	[PROC_PROTMAX_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = protmax_status, .copyout_on_error = false, },
	[PROC_STACKGAP_CTL] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = stackgap_ctl, .copyout_on_error = false, },
	[PROC_STACKGAP_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = stackgap_status, .copyout_on_error = false, },
	[PROC_NO_NEW_PRIVS_CTL] =
	    { .lock_tree = PCTL_SLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = no_new_privs_ctl, .copyout_on_error = false, },
	[PROC_NO_NEW_PRIVS_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = no_new_privs_status, .copyout_on_error = false, },
	[PROC_WXMAP_CTL] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = true,
	      .copyin_sz = sizeof(int), .copyout_sz = 0,
	      .exec = wxmap_ctl, .copyout_on_error = false, },
	[PROC_WXMAP_STATUS] =
	    { .lock_tree = PCTL_UNLOCKED, .one_proc = true,
	      .esrch_is_einval = false, .no_nonnull_data = false,
	      .need_candebug = false,
	      .copyin_sz = 0, .copyout_sz = sizeof(int),
	      .exec = wxmap_status, .copyout_on_error = false, },
};

int
sys_procctl(struct thread *td, struct procctl_args *uap)
{
	union {
		struct procctl_reaper_status rs;
		struct procctl_reaper_pids rp;
		struct procctl_reaper_kill rk;
		int flags;
	} x;
	const struct procctl_cmd_info *cmd_info;
	int error, error1;

	if (uap->com >= PROC_PROCCTL_MD_MIN)
		return (cpu_procctl(td, uap->idtype, uap->id,
		    uap->com, uap->data));
	if (uap->com <= 0 || uap->com >= nitems(procctl_cmds_info))
		return (EINVAL);
	cmd_info = &procctl_cmds_info[uap->com];
	bzero(&x, sizeof(x));

	if (cmd_info->copyin_sz > 0) {
		error = copyin(uap->data, &x, cmd_info->copyin_sz);
		if (error != 0)
			return (error);
	} else if (cmd_info->no_nonnull_data && uap->data != NULL) {
		return (EINVAL);
	}

	error = kern_procctl(td, uap->idtype, uap->id, uap->com, &x);

	if (cmd_info->copyout_sz > 0 && (error == 0 ||
	    cmd_info->copyout_on_error)) {
		error1 = copyout(&x, uap->data, cmd_info->copyout_sz);
		if (error == 0)
			error = error1;
	}
	return (error);
}

static int
kern_procctl_single(struct thread *td, struct proc *p, int com, void *data)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	return (procctl_cmds_info[com].exec(td, p, data));
}

int
kern_procctl(struct thread *td, idtype_t idtype, id_t id, int com, void *data)
{
	struct pgrp *pg;
	struct proc *p;
	const struct procctl_cmd_info *cmd_info;
	int error, first_error, ok;
	bool sapblk;

	MPASS(com > 0 && com < nitems(procctl_cmds_info));
	cmd_info = &procctl_cmds_info[com];
	if (idtype != P_PID && cmd_info->one_proc)
		return (EINVAL);

	sapblk = false;
	if (cmd_info->sapblk != NULL) {
		sapblk = cmd_info->sapblk(td, data);
		if (sapblk && !stop_all_proc_block())
			return (ERESTART);
	}

	switch (cmd_info->lock_tree) {
	case PCTL_XLOCKED:
		sx_xlock(&proctree_lock);
		break;
	case PCTL_SLOCKED:
		sx_slock(&proctree_lock);
		break;
	default:
		break;
	}

	switch (idtype) {
	case P_PID:
		if (id == 0) {
			p = td->td_proc;
			error = 0;
			PROC_LOCK(p);
		} else {
			p = pfind(id);
			if (p == NULL) {
				error = cmd_info->esrch_is_einval ?
				    EINVAL : ESRCH;
				break;
			}
			error = cmd_info->need_candebug ? p_candebug(td, p) :
			    p_cansee(td, p);
		}
		if (error == 0)
			error = kern_procctl_single(td, p, com, data);
		PROC_UNLOCK(p);
		break;
	case P_PGID:
		/*
		 * Attempt to apply the operation to all members of the
		 * group.  Ignore processes in the group that can't be
		 * seen.  Ignore errors so long as at least one process is
		 * able to complete the request successfully.
		 */
		pg = pgfind(id);
		if (pg == NULL) {
			error = ESRCH;
			break;
		}
		PGRP_UNLOCK(pg);
		ok = 0;
		first_error = 0;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NEW ||
			    p->p_state == PRS_ZOMBIE ||
			    (cmd_info->need_candebug ? p_candebug(td, p) :
			    p_cansee(td, p)) != 0) {
				PROC_UNLOCK(p);
				continue;
			}
			error = kern_procctl_single(td, p, com, data);
			PROC_UNLOCK(p);
			if (error == 0)
				ok = 1;
			else if (first_error == 0)
				first_error = error;
		}
		if (ok)
			error = 0;
		else if (first_error != 0)
			error = first_error;
		else
			/*
			 * Was not able to see any processes in the
			 * process group.
			 */
			error = ESRCH;
		break;
	default:
		error = EINVAL;
		break;
	}

	switch (cmd_info->lock_tree) {
	case PCTL_XLOCKED:
		sx_xunlock(&proctree_lock);
		break;
	case PCTL_SLOCKED:
		sx_sunlock(&proctree_lock);
		break;
	default:
		break;
	}
	if (sapblk)
		stop_all_proc_unblock();
	return (error);
}
