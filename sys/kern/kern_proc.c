/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_proc.c	8.7 (Berkeley) 2/14/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/jail.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>

MALLOC_DEFINE(M_PGRP, "pgrp", "process group header");
MALLOC_DEFINE(M_SESSION, "session", "session header");
static MALLOC_DEFINE(M_PROC, "proc", "Proc structures");
MALLOC_DEFINE(M_SUBPROC, "subproc", "Proc sub-structures");

static struct proc *dopfind	(register pid_t);

static void doenterpgrp	(struct proc *, struct pgrp *);

static void pgdelete	(struct pgrp *);

static void orphanpg	(struct pgrp *pg);

/*
 * Other process lists
 */
struct pidhashhead *pidhashtbl;
u_long pidhash;
struct pgrphashhead *pgrphashtbl;
u_long pgrphash;
struct proclist allproc;
struct proclist zombproc;
struct sx allproc_lock;
struct sx proctree_lock;
struct sx pgrpsess_lock;
uma_zone_t proc_zone;
uma_zone_t ithread_zone;

/*
 * Initialize global process hashing structures.
 */
void
procinit()
{
	int i, j;

	sx_init(&allproc_lock, "allproc");
	sx_init(&proctree_lock, "proctree");
	sx_init(&pgrpsess_lock, "pgrpsess");
	LIST_INIT(&allproc);
	LIST_INIT(&zombproc);
	pidhashtbl = hashinit(maxproc / 4, M_PROC, &pidhash);
	pgrphashtbl = hashinit(maxproc / 4, M_PROC, &pgrphash);
	proc_zone = uma_zcreate("PROC", sizeof (struct proc), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uihashinit();
	/*
	 * This should really be a compile time warning, but I do
	 * not know of any way to do that...
	 */
	if (sizeof(struct kinfo_proc) != KINFO_PROC_SIZE) {
		printf("This message will repeat for the next 20 seconds\n");
		for (i = 0; i < 20; i++) {
			printf("WARNING: size of kinfo_proc (%ld) should be %d!!!\n",
			    (long)sizeof(struct kinfo_proc), KINFO_PROC_SIZE);
			printf("The kinfo_proc structure was changed ");
			printf("incorrectly in <sys/user.h>\n");
			for (j = 0; j < 0x7ffffff; j++);
		}

	}
}

/*
 * Note that we do not link to the proc's ucred here
 * The thread is linked as if running but no KSE assigned
 */
static  void
thread_link(struct thread *td, struct ksegrp *kg)
{
	struct proc *p = kg->kg_proc;

	td->td_proc     = p;
	td->td_ksegrp   = kg;
	td->td_last_kse = &p->p_kse;

	TAILQ_INSERT_HEAD(&p->p_threads, td, td_plist);
	TAILQ_INSERT_HEAD(&kg->kg_threads, td, td_kglist);
	td->td_critnest = 0;
	td->td_savecrit = 0;
	td->td_kse      = NULL;
}

/* 
 * KSE is linked onto the idle queue.
 */
static void
kse_link(struct kse *ke, struct ksegrp *kg)
{
	struct proc *p = kg->kg_proc;

	TAILQ_INSERT_HEAD(&kg->kg_kseq, ke, ke_kglist);
	kg->kg_kses++;
	TAILQ_INSERT_HEAD(&kg->kg_iq, ke, ke_kgrlist);
	ke->ke_proc	= p;
	ke->ke_ksegrp	= kg;
	ke->ke_thread	= NULL;
	ke->ke_oncpu = NOCPU;
}

static void
ksegrp_link(struct ksegrp *kg, struct proc *p)
{

	TAILQ_INIT(&kg->kg_threads);
	TAILQ_INIT(&kg->kg_runq);	/* links with td_runq */
	TAILQ_INIT(&kg->kg_slpq);	/* links with td_runq */
	TAILQ_INIT(&kg->kg_kseq);	/* all kses in ksegrp */
	TAILQ_INIT(&kg->kg_iq);		/* all kses in ksegrp */
	kg->kg_proc	= p;
/* the following counters are in the -zero- section and may not need clearing */
	kg->kg_runnable = 0;
	kg->kg_kses = 0;
	kg->kg_runq_kses = 0; /* XXXKSE change name */
/* link it in now that it's consitant */
	TAILQ_INSERT_HEAD(&p->p_ksegrps, kg, kg_ksegrp);
}

/*
 * for a newly created process,
 * link up a the structure and its initial threads etc.
 */
void
proc_linkup(struct proc *p, struct ksegrp *kg,
			struct kse *ke, struct thread *td)
{

	TAILQ_INIT(&p->p_ksegrps);	     /* all ksegrps in proc */
	TAILQ_INIT(&p->p_threads);	     /* all threads in proc */

	ksegrp_link(kg, p);
	kse_link(ke, kg);
	thread_link(td, kg);
	/* link them together for 1:1 */
	td->td_kse = ke;
	ke->ke_thread = td;
}

/* temporary version is ultra simple while we are in 1:1 mode */
struct thread *
thread_get(struct proc *p)
{
	struct thread *td = &p->p_xxthread;

	return (td);
}


/*********************
* STUB KSE syscalls
*********************/

/* struct thread_wakeup_args { struct thread_mailbox *tmbx; }; */
int
thread_wakeup(struct thread *td, struct  thread_wakeup_args *uap)
{

	return(ENOSYS);
}

int
kse_exit(struct thread *td, struct kse_exit_args *uap)
{

	return(ENOSYS);
}

int
kse_yield(struct thread *td, struct kse_yield_args *uap)
{

	return(ENOSYS);
}

int kse_wakeup(struct thread *td, struct kse_wakeup_args *uap)
{

	return(ENOSYS);
}


int
kse_new(struct thread *td, struct kse_new_args *uap)
/* struct kse_new_args {
	struct kse_mailbox *mbx;
	int	new_grp_flag;
}; */
{

	return (ENOSYS);
}

/*
 * Is p an inferior of the current process?
 */
int
inferior(p)
	register struct proc *p;
{

	sx_assert(&proctree_lock, SX_LOCKED);
	for (; p != curproc; p = p->p_pptr)
		if (p->p_pid == 0)
			return (0);
	return (1);
}

/*
 * Locate a process by number
 */
struct proc *
pfind(pid)
	register pid_t pid;
{
	register struct proc *p;

	sx_slock(&allproc_lock);
	p = dopfind(pid);
	sx_sunlock(&allproc_lock);
	return (p);
}

static struct proc *
dopfind(pid)
	register pid_t pid;
{
	register struct proc *p;

	sx_assert(&allproc_lock, SX_LOCKED);

	LIST_FOREACH(p, PIDHASH(pid), p_hash)
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			break;
		}
	return (p);
}

/*
 * Locate a process group by number.
 * The caller must hold pgrpsess_lock.
 */
struct pgrp *
pgfind(pgid)
	register pid_t pgid;
{
	register struct pgrp *pgrp;

	PGRPSESS_LOCK_ASSERT(SX_LOCKED);

	LIST_FOREACH(pgrp, PGRPHASH(pgid), pg_hash) {
		if (pgrp->pg_id == pgid) {
			PGRP_LOCK(pgrp);
			return (pgrp);
		}
	}
	return (NULL);
}

/*
 * Create a new process group.
 * pgid must be equal to the pid of p.
 * Begin a new session if required.
 */
int
enterpgrp(p, pgid, pgrp, sess)
	register struct proc *p;
	pid_t pgid;
	struct pgrp *pgrp;
	struct session *sess;
{
	struct pgrp *pgrp2;

	PGRPSESS_LOCK_ASSERT(SX_XLOCKED);

	KASSERT(pgrp != NULL, ("enterpgrp: pgrp == NULL"));
	KASSERT(p->p_pid == pgid,
	    ("enterpgrp: new pgrp and pid != pgid"));

	pgrp2 = pgfind(pgid);

	KASSERT(pgrp2 == NULL,
	    ("enterpgrp: pgrp with pgid exists"));
	KASSERT(!SESS_LEADER(p),
	    ("enterpgrp: session leader attempted setpgrp"));

	mtx_init(&pgrp->pg_mtx, "process group", MTX_DEF);

	if (sess != NULL) {
		/*
		 * new session
		 */
		mtx_init(&sess->s_mtx, "session", MTX_DEF);
		PROC_LOCK(p);
		p->p_flag &= ~P_CONTROLT;
		PROC_UNLOCK(p);
		PGRP_LOCK(pgrp);
		sess->s_leader = p;
		sess->s_sid = p->p_pid;
		sess->s_count = 1;
		sess->s_ttyvp = NULL;
		sess->s_ttyp = NULL;
		bcopy(p->p_session->s_login, sess->s_login,
			    sizeof(sess->s_login));
		pgrp->pg_session = sess;
		KASSERT(p == curproc,
		    ("enterpgrp: mksession and p != curproc"));
	} else {
		pgrp->pg_session = p->p_session;
		SESS_LOCK(pgrp->pg_session);
		pgrp->pg_session->s_count++;
		SESS_UNLOCK(pgrp->pg_session);
		PGRP_LOCK(pgrp);
	}
	pgrp->pg_id = pgid;
	LIST_INIT(&pgrp->pg_members);

	/*
	 * As we have an exclusive lock of pgrpsess_lock,
	 * this should not deadlock.
	 */
	LIST_INSERT_HEAD(PGRPHASH(pgid), pgrp, pg_hash);
	pgrp->pg_jobc = 0;
	SLIST_INIT(&pgrp->pg_sigiolst);
	PGRP_UNLOCK(pgrp);

	doenterpgrp(p, pgrp);

	return (0);
}

/*
 * Move p to an existing process group
 */
int
enterthispgrp(p, pgrp)
	register struct proc *p;
	struct pgrp *pgrp;
{
	PGRPSESS_LOCK_ASSERT(SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);
	KASSERT(pgrp->pg_session == p->p_session,
		("%s: pgrp's session %p, p->p_session %p.\n",
		__func__,
		pgrp->pg_session,
		p->p_session));
	KASSERT(pgrp != p->p_pgrp,
		("%s: p belongs to pgrp.", __func__));

	doenterpgrp(p, pgrp);

	return (0);
}

/*
 * Move p to a process group
 */
static void
doenterpgrp(p, pgrp)
	struct proc *p;
	struct pgrp *pgrp;
{
	struct pgrp *savepgrp;

	PGRPSESS_LOCK_ASSERT(SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);

	savepgrp = p->p_pgrp;

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	PGRP_LOCK(pgrp);
	PGRP_LOCK(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = pgrp;
	PROC_UNLOCK(p);
	LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
	PGRP_UNLOCK(savepgrp);
	PGRP_UNLOCK(pgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
}

/*
 * remove process from process group
 */
int
leavepgrp(p)
	register struct proc *p;
{
	struct pgrp *savepgrp;

	PGRPSESS_XLOCK();
	savepgrp = p->p_pgrp;
	PGRP_LOCK(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = NULL;
	PROC_UNLOCK(p);
	PGRP_UNLOCK(savepgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
	PGRPSESS_XUNLOCK();
	return (0);
}

/*
 * delete a process group
 */
static void
pgdelete(pgrp)
	register struct pgrp *pgrp;
{
	struct session *savesess;

	PGRPSESS_LOCK_ASSERT(SX_XLOCKED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	PGRP_LOCK(pgrp);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pgid.
	 */
	funsetownlst(&pgrp->pg_sigiolst);

	if (pgrp->pg_session->s_ttyp != NULL &&
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	LIST_REMOVE(pgrp, pg_hash);
	savesess = pgrp->pg_session;
	SESS_LOCK(savesess);
	savesess->s_count--;
	SESS_UNLOCK(savesess);
	PGRP_UNLOCK(pgrp);
	if (savesess->s_count == 0) {
		mtx_destroy(&savesess->s_mtx);
		FREE(pgrp->pg_session, M_SESSION);
	}
	mtx_destroy(&pgrp->pg_mtx);
	FREE(pgrp, M_PGRP);
}

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(p, pgrp, entering)
	register struct proc *p;
	register struct pgrp *pgrp;
	int entering;
{
	register struct pgrp *hispgrp;
	register struct session *mysession;

	PGRPSESS_LOCK_ASSERT(SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	/*
	 * Check p's parent to see whether p qualifies its own process
	 * group; if so, adjust count for p's process group.
	 */
	mysession = pgrp->pg_session;
	sx_slock(&proctree_lock);
	if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
	    hispgrp->pg_session == mysession) {
		PGRP_LOCK(pgrp);
		if (entering)
			pgrp->pg_jobc++;
		else {
			--pgrp->pg_jobc;
			if (pgrp->pg_jobc == 0)
				orphanpg(pgrp);
		}
		PGRP_UNLOCK(pgrp);
	}

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups; if so, adjust counts for children's
	 * process groups.
	 */
	LIST_FOREACH(p, &p->p_children, p_sibling) {
		if ((hispgrp = p->p_pgrp) != pgrp &&
		    hispgrp->pg_session == mysession &&
		    p->p_stat != SZOMB) {
			PGRP_LOCK(hispgrp);
			if (entering)
				hispgrp->pg_jobc++;
			else {
				--hispgrp->pg_jobc;
				if (hispgrp->pg_jobc == 0)
					orphanpg(hispgrp);
			}
			PGRP_UNLOCK(hispgrp);
		}
	}
	sx_sunlock(&proctree_lock);
}

/*
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(pg)
	struct pgrp *pg;
{
	register struct proc *p;

	PGRP_LOCK_ASSERT(pg, MA_OWNED);

	mtx_lock_spin(&sched_lock);
	LIST_FOREACH(p, &pg->pg_members, p_pglist) {
		if (p->p_stat == SSTOP) {
			mtx_unlock_spin(&sched_lock);
			LIST_FOREACH(p, &pg->pg_members, p_pglist) {
				PROC_LOCK(p);
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
				PROC_UNLOCK(p);
			}
			return;
		}
	}
	mtx_unlock_spin(&sched_lock);
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(pgrpdump, pgrpdump)
{
	register struct pgrp *pgrp;
	register struct proc *p;
	register int i;

	for (i = 0; i <= pgrphash; i++) {
		if (!LIST_EMPTY(&pgrphashtbl[i])) {
			printf("\tindx %d\n", i);
			LIST_FOREACH(pgrp, &pgrphashtbl[i], pg_hash) {
				printf(
			"\tpgrp %p, pgid %ld, sess %p, sesscnt %d, mem %p\n",
				    (void *)pgrp, (long)pgrp->pg_id,
				    (void *)pgrp->pg_session,
				    pgrp->pg_session->s_count,
				    (void *)LIST_FIRST(&pgrp->pg_members));
				LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
					printf("\t\tpid %ld addr %p pgrp %p\n", 
					    (long)p->p_pid, (void *)p,
					    (void *)p->p_pgrp);
				}
			}
		}
	}
}
#endif /* DDB */

/*
 * Fill in an kinfo_proc structure for the specified process.
 */
void
fill_kinfo_proc(p, kp)
	struct proc *p;
	struct kinfo_proc *kp;
{
	struct thread *td;
	struct tty *tp;
	struct session *sp;
	struct timeval tv;

	bzero(kp, sizeof(*kp));

	kp->ki_structsize = sizeof(*kp);
	kp->ki_paddr = p;
	PROC_LOCK(p);
	kp->ki_addr =/* p->p_addr; */0; /* XXXKSE */
	kp->ki_args = p->p_args;
	kp->ki_tracep = p->p_tracep;
	kp->ki_textvp = p->p_textvp;
	kp->ki_fd = p->p_fd;
	kp->ki_vmspace = p->p_vmspace;
	if (p->p_ucred) {
		kp->ki_uid = p->p_ucred->cr_uid;
		kp->ki_ruid = p->p_ucred->cr_ruid;
		kp->ki_svuid = p->p_ucred->cr_svuid;
		/* XXX bde doesn't like KI_NGROUPS */
		kp->ki_ngroups = min(p->p_ucred->cr_ngroups, KI_NGROUPS);
		bcopy(p->p_ucred->cr_groups, kp->ki_groups,
		    kp->ki_ngroups * sizeof(gid_t));
		kp->ki_rgid = p->p_ucred->cr_rgid;
		kp->ki_svgid = p->p_ucred->cr_svgid;
	}
	if (p->p_procsig) {
		kp->ki_sigignore = p->p_procsig->ps_sigignore;
		kp->ki_sigcatch = p->p_procsig->ps_sigcatch;
	}
	mtx_lock_spin(&sched_lock);
	if (p->p_stat != SIDL && p->p_stat != SZOMB && p->p_vmspace != NULL) {
		struct vmspace *vm = p->p_vmspace;

		kp->ki_size = vm->vm_map.size;
		kp->ki_rssize = vmspace_resident_count(vm); /*XXX*/
		if (p->p_sflag & PS_INMEM)
			kp->ki_rssize += UAREA_PAGES;
		FOREACH_THREAD_IN_PROC(p, td) /* XXXKSE: thread swapout check */
			kp->ki_rssize += KSTACK_PAGES;
		kp->ki_swrss = vm->vm_swrss;
		kp->ki_tsize = vm->vm_tsize;
		kp->ki_dsize = vm->vm_dsize;
		kp->ki_ssize = vm->vm_ssize;
	}
	if ((p->p_sflag & PS_INMEM) && p->p_stats) {
		kp->ki_start = p->p_stats->p_start;
		kp->ki_rusage = p->p_stats->p_ru;
		kp->ki_childtime.tv_sec = p->p_stats->p_cru.ru_utime.tv_sec +
		    p->p_stats->p_cru.ru_stime.tv_sec;
		kp->ki_childtime.tv_usec = p->p_stats->p_cru.ru_utime.tv_usec +
		    p->p_stats->p_cru.ru_stime.tv_usec;
	}
	td = FIRST_THREAD_IN_PROC(p);
	if (td->td_wmesg != NULL)
		strncpy(kp->ki_wmesg, td->td_wmesg, sizeof(kp->ki_wmesg) - 1);
	if (p->p_stat == SMTX) {
		kp->ki_kiflag |= KI_MTXBLOCK;
		strncpy(kp->ki_mtxname, td->td_mtxname,
		    sizeof(kp->ki_mtxname) - 1);
	}
	kp->ki_stat = p->p_stat;
	kp->ki_sflag = p->p_sflag;
	kp->ki_swtime = p->p_swtime;
	kp->ki_traceflag = p->p_traceflag;
	kp->ki_pid = p->p_pid;
	/* vvv XXXKSE */
	bintime2timeval(&p->p_runtime, &tv);
	kp->ki_runtime = tv.tv_sec * (u_int64_t)1000000 + tv.tv_usec;
	kp->ki_pctcpu = p->p_kse.ke_pctcpu;
	kp->ki_estcpu = td->td_ksegrp->kg_estcpu;
	kp->ki_slptime = td->td_ksegrp->kg_slptime;
	kp->ki_wchan = td->td_wchan;
	kp->ki_pri.pri_level = td->td_priority;
	kp->ki_pri.pri_user = td->td_ksegrp->kg_user_pri;
	kp->ki_pri.pri_class = td->td_ksegrp->kg_pri_class;
	kp->ki_pri.pri_native = td->td_base_pri;
	kp->ki_nice = td->td_ksegrp->kg_nice;
	kp->ki_rqindex = p->p_kse.ke_rqindex;
	kp->ki_oncpu = p->p_kse.ke_oncpu;
	kp->ki_lastcpu = td->td_lastcpu;
	kp->ki_tdflags = td->td_flags;
	kp->ki_pcb = td->td_pcb;
	kp->ki_kstack = (void *)td->td_kstack;
	/* ^^^ XXXKSE */
	mtx_unlock_spin(&sched_lock);
	sp = NULL;
	tp = NULL;
	if (p->p_pgrp) {
		kp->ki_pgid = p->p_pgrp->pg_id;
		kp->ki_jobc = p->p_pgrp->pg_jobc;
		sp = p->p_pgrp->pg_session;

		if (sp != NULL) {
			kp->ki_sid = sp->s_sid;
			SESS_LOCK(sp);
			strncpy(kp->ki_login, sp->s_login,
			    sizeof(kp->ki_login) - 1);
			if (sp->s_ttyvp)
				kp->ki_kiflag |= KI_CTTY;
			if (SESS_LEADER(p))
				kp->ki_kiflag |= KI_SLEADER;
			tp = sp->s_ttyp;
			SESS_UNLOCK(sp);
		}
	}
	if ((p->p_flag & P_CONTROLT) && tp != NULL) {
		kp->ki_tdev = dev2udev(tp->t_dev);
		kp->ki_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		if (tp->t_session)
			kp->ki_tsid = tp->t_session->s_sid;
	} else
		kp->ki_tdev = NOUDEV;
	if (p->p_comm[0] != '\0') {
		strncpy(kp->ki_comm, p->p_comm, sizeof(kp->ki_comm) - 1);
		strncpy(kp->ki_ocomm, p->p_comm, sizeof(kp->ki_ocomm) - 1);
	}
	kp->ki_siglist = p->p_siglist;
	kp->ki_sigmask = p->p_sigmask;
	kp->ki_xstat = p->p_xstat;
	kp->ki_acflag = p->p_acflag;
	kp->ki_flag = p->p_flag;
	/* If jailed(p->p_ucred), emulate the old P_JAILED flag. */
	if (jailed(p->p_ucred))
		kp->ki_flag |= P_JAILED;
	kp->ki_lock = p->p_lock;
	if (p->p_pptr)
		kp->ki_ppid = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
}

/*
 * Locate a zombie process by number
 */
struct proc *
zpfind(pid_t pid)
{
	struct proc *p;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &zombproc, p_list)
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			break;
		}
	sx_sunlock(&allproc_lock);
	return (p);
}


static int
sysctl_out_proc(struct proc *p, struct sysctl_req *req, int doingzomb)
{
	struct kinfo_proc kinfo_proc;
	int error;
	struct proc *np;
	pid_t pid = p->p_pid;

	fill_kinfo_proc(p, &kinfo_proc);
	error = SYSCTL_OUT(req, (caddr_t)&kinfo_proc, sizeof(kinfo_proc));
	if (error)
		return (error);
	if (doingzomb)
		np = zpfind(pid);
	else {
		if (pid == 0)
			return (0);
		np = pfind(pid);
	}
	if (np == NULL)
		return EAGAIN;
	if (np != p) {
		PROC_UNLOCK(np);
		return EAGAIN;
	}
	PROC_UNLOCK(np);
	return (0);
}

static int
sysctl_kern_proc(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	int doingzomb;
	int error = 0;

	if (oidp->oid_number == KERN_PROC_PID) {
		if (namelen != 1) 
			return (EINVAL);
		p = pfind((pid_t)name[0]);
		if (!p)
			return (0);
		if (p_cansee(curproc, p)) {
			PROC_UNLOCK(p);
			return (0);
		}
		PROC_UNLOCK(p);
		error = sysctl_out_proc(p, req, 0);
		return (error);
	}
	if (oidp->oid_number == KERN_PROC_ALL && !namelen)
		;
	else if (oidp->oid_number != KERN_PROC_ALL && namelen == 1)
		;
	else
		return (EINVAL);
	
	if (!req->oldptr) {
		/* overestimate by 5 procs */
		error = SYSCTL_OUT(req, 0, sizeof (struct kinfo_proc) * 5);
		if (error)
			return (error);
	}
	sx_slock(&allproc_lock);
	for (doingzomb=0 ; doingzomb < 2 ; doingzomb++) {
		if (!doingzomb)
			p = LIST_FIRST(&allproc);
		else
			p = LIST_FIRST(&zombproc);
		for (; p != 0; p = LIST_NEXT(p, p_list)) {
			/*
			 * Show a user only appropriate processes.
			 */
			if (p_cansee(curproc, p))
				continue;
			/*
			 * Skip embryonic processes.
			 */
			if (p->p_stat == SIDL)
				continue;
			/*
			 * TODO - make more efficient (see notes below).
			 * do by session.
			 */
			switch (oidp->oid_number) {

			case KERN_PROC_PGRP:
				/* could do this by traversing pgrp */
				PROC_LOCK(p);
				if (p->p_pgrp == NULL || 
				    p->p_pgrp->pg_id != (pid_t)name[0]) {
					PROC_UNLOCK(p);
					continue;
				}
				PROC_UNLOCK(p);
				break;

			case KERN_PROC_TTY:
				PROC_LOCK(p);
				if ((p->p_flag & P_CONTROLT) == 0 ||
				    p->p_session == NULL) {
					PROC_UNLOCK(p);
					continue;
				}
				SESS_LOCK(p->p_session);
				if (p->p_session->s_ttyp == NULL ||
				    dev2udev(p->p_session->s_ttyp->t_dev) != 
				    (udev_t)name[0]) {
					SESS_UNLOCK(p->p_session);
					PROC_UNLOCK(p);
					continue;
				}
				SESS_UNLOCK(p->p_session);
				PROC_UNLOCK(p);
				break;

			case KERN_PROC_UID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_uid != (uid_t)name[0])
					continue;
				break;

			case KERN_PROC_RUID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_ruid != (uid_t)name[0])
					continue;
				break;
			}

			if (p_cansee(curproc, p))
				continue;

			error = sysctl_out_proc(p, req, doingzomb);
			if (error) {
				sx_sunlock(&allproc_lock);
				return (error);
			}
		}
	}
	sx_sunlock(&allproc_lock);
	return (0);
}

/*
 * This sysctl allows a process to retrieve the argument list or process
 * title for another process without groping around in the address space
 * of the other process.  It also allow a process to set its own "process 
 * title to a string of its own choice.
 */
static int
sysctl_kern_proc_args(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct pargs *pa;
	int error = 0;

	if (namelen != 1) 
		return (EINVAL);

	p = pfind((pid_t)name[0]);
	if (!p)
		return (0);

	if ((!ps_argsopen) && p_cansee(curproc, p)) {
		PROC_UNLOCK(p);
		return (0);
	}
	PROC_UNLOCK(p);

	if (req->newptr && curproc != p)
		return (EPERM);

	if (req->oldptr && p->p_args != NULL)
		error = SYSCTL_OUT(req, p->p_args->ar_args, p->p_args->ar_length);
	if (req->newptr == NULL)
		return (error);

	PROC_LOCK(p);
	pa = p->p_args;
	p->p_args = NULL;
	PROC_UNLOCK(p);
	if (pa != NULL && --pa->ar_ref == 0) 
		FREE(pa, M_PARGS);

	if (req->newlen + sizeof(struct pargs) > ps_arg_cache_limit)
		return (error);

	MALLOC(pa, struct pargs *, sizeof(struct pargs) + req->newlen, 
	    M_PARGS, M_WAITOK);
	pa->ar_ref = 1;
	pa->ar_length = req->newlen;
	error = SYSCTL_IN(req, pa->ar_args, req->newlen);
	if (!error) {
		PROC_LOCK(p);
		p->p_args = pa;
		PROC_UNLOCK(p);
	} else
		FREE(pa, M_PARGS);
	return (error);
}

SYSCTL_NODE(_kern, KERN_PROC, proc, CTLFLAG_RD,  0, "Process table");

SYSCTL_PROC(_kern_proc, KERN_PROC_ALL, all, CTLFLAG_RD|CTLTYPE_STRUCT,
	0, 0, sysctl_kern_proc, "S,proc", "Return entire process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PGRP, pgrp, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_TTY, tty, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_UID, uid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_RUID, ruid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PID, pid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_ARGS, args, CTLFLAG_RW | CTLFLAG_ANYBODY,
	sysctl_kern_proc_args, "Process argument list");
