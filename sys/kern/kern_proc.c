/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_ktrace.h"
#include "opt_kstack_pages.h"
#include "opt_stack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/conf.h>
#include <sys/elf.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/loginclass.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sysent.h>
#include <sys/sched.h>
#include <sys/shm.h>
#include <sys/smp.h>
#include <sys/stack.h>
#include <sys/stat.h>
#include <sys/dtrace_bsd.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/sdt.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/uma.h>

#include <fs/devfs/devfs.h>

#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_util.h>
#endif

SDT_PROVIDER_DEFINE(proc);

MALLOC_DEFINE(M_SESSION, "session", "session header");
static MALLOC_DEFINE(M_PROC, "proc", "Proc structures");
MALLOC_DEFINE(M_SUBPROC, "subproc", "Proc sub-structures");

static void doenterpgrp(struct proc *, struct pgrp *);
static void orphanpg(struct pgrp *pg);
static void fill_kinfo_aggregate(struct proc *p, struct kinfo_proc *kp);
static void fill_kinfo_proc_only(struct proc *p, struct kinfo_proc *kp);
static void fill_kinfo_thread(struct thread *td, struct kinfo_proc *kp,
    int preferthread);
static void pgdelete(struct pgrp *);
static int pgrp_init(void *mem, int size, int flags);
static int proc_ctor(void *mem, int size, void *arg, int flags);
static void proc_dtor(void *mem, int size, void *arg);
static int proc_init(void *mem, int size, int flags);
static void proc_fini(void *mem, int size);
static void pargs_free(struct pargs *pa);

/*
 * Other process lists
 */
struct pidhashhead *pidhashtbl = NULL;
struct sx *pidhashtbl_lock;
u_long pidhash;
u_long pidhashlock;
struct pgrphashhead *pgrphashtbl;
u_long pgrphash;
struct proclist allproc = LIST_HEAD_INITIALIZER(allproc);
struct sx __exclusive_cache_line allproc_lock;
struct sx __exclusive_cache_line proctree_lock;
struct mtx __exclusive_cache_line ppeers_lock;
struct mtx __exclusive_cache_line procid_lock;
uma_zone_t proc_zone;
uma_zone_t pgrp_zone;

/*
 * The offset of various fields in struct proc and struct thread.
 * These are used by kernel debuggers to enumerate kernel threads and
 * processes.
 */
const int proc_off_p_pid = offsetof(struct proc, p_pid);
const int proc_off_p_comm = offsetof(struct proc, p_comm);
const int proc_off_p_list = offsetof(struct proc, p_list);
const int proc_off_p_hash = offsetof(struct proc, p_hash);
const int proc_off_p_threads = offsetof(struct proc, p_threads);
const int thread_off_td_tid = offsetof(struct thread, td_tid);
const int thread_off_td_name = offsetof(struct thread, td_name);
const int thread_off_td_oncpu = offsetof(struct thread, td_oncpu);
const int thread_off_td_pcb = offsetof(struct thread, td_pcb);
const int thread_off_td_plist = offsetof(struct thread, td_plist);

EVENTHANDLER_LIST_DEFINE(process_ctor);
EVENTHANDLER_LIST_DEFINE(process_dtor);
EVENTHANDLER_LIST_DEFINE(process_init);
EVENTHANDLER_LIST_DEFINE(process_fini);
EVENTHANDLER_LIST_DEFINE(process_exit);
EVENTHANDLER_LIST_DEFINE(process_fork);
EVENTHANDLER_LIST_DEFINE(process_exec);

int kstack_pages = KSTACK_PAGES;
SYSCTL_INT(_kern, OID_AUTO, kstack_pages, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &kstack_pages, 0,
    "Kernel stack size in pages");
static int vmmap_skip_res_cnt = 0;
SYSCTL_INT(_kern, OID_AUTO, proc_vmmap_skip_resident_count, CTLFLAG_RW,
    &vmmap_skip_res_cnt, 0,
    "Skip calculation of the pages resident count in kern.proc.vmmap");

CTASSERT(sizeof(struct kinfo_proc) == KINFO_PROC_SIZE);
#ifdef COMPAT_FREEBSD32
CTASSERT(sizeof(struct kinfo_proc32) == KINFO_PROC32_SIZE);
#endif

/*
 * Initialize global process hashing structures.
 */
void
procinit(void)
{
	u_long i;

	sx_init(&allproc_lock, "allproc");
	sx_init(&proctree_lock, "proctree");
	mtx_init(&ppeers_lock, "p_peers", NULL, MTX_DEF);
	mtx_init(&procid_lock, "procid", NULL, MTX_DEF);
	pidhashtbl = hashinit(maxproc / 4, M_PROC, &pidhash);
	pidhashlock = (pidhash + 1) / 64;
	if (pidhashlock > 0)
		pidhashlock--;
	pidhashtbl_lock = malloc(sizeof(*pidhashtbl_lock) * (pidhashlock + 1),
	    M_PROC, M_WAITOK | M_ZERO);
	for (i = 0; i < pidhashlock + 1; i++)
		sx_init_flags(&pidhashtbl_lock[i], "pidhash", SX_DUPOK);
	pgrphashtbl = hashinit(maxproc / 4, M_PROC, &pgrphash);
	proc_zone = uma_zcreate("PROC", sched_sizeof_proc(),
	    proc_ctor, proc_dtor, proc_init, proc_fini,
	    UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	pgrp_zone = uma_zcreate("PGRP", sizeof(struct pgrp), NULL, NULL,
	    pgrp_init, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uihashinit();
}

/*
 * Prepare a proc for use.
 */
static int
proc_ctor(void *mem, int size, void *arg, int flags)
{
	struct proc *p;
	struct thread *td;

	p = (struct proc *)mem;
#ifdef KDTRACE_HOOKS
	kdtrace_proc_ctor(p);
#endif
	EVENTHANDLER_DIRECT_INVOKE(process_ctor, p);
	td = FIRST_THREAD_IN_PROC(p);
	if (td != NULL) {
		/* Make sure all thread constructors are executed */
		EVENTHANDLER_DIRECT_INVOKE(thread_ctor, td);
	}
	return (0);
}

/*
 * Reclaim a proc after use.
 */
static void
proc_dtor(void *mem, int size, void *arg)
{
	struct proc *p;
	struct thread *td;

	/* INVARIANTS checks go here */
	p = (struct proc *)mem;
	td = FIRST_THREAD_IN_PROC(p);
	if (td != NULL) {
#ifdef INVARIANTS
		KASSERT((p->p_numthreads == 1),
		    ("bad number of threads in exiting process"));
		KASSERT(STAILQ_EMPTY(&p->p_ktr), ("proc_dtor: non-empty p_ktr"));
#endif
		/* Free all OSD associated to this thread. */
		osd_thread_exit(td);
		ast_kclear(td);

		/* Make sure all thread destructors are executed */
		EVENTHANDLER_DIRECT_INVOKE(thread_dtor, td);
	}
	EVENTHANDLER_DIRECT_INVOKE(process_dtor, p);
#ifdef KDTRACE_HOOKS
	kdtrace_proc_dtor(p);
#endif
	if (p->p_ksi != NULL)
		KASSERT(! KSI_ONQ(p->p_ksi), ("SIGCHLD queue"));
}

/*
 * Initialize type-stable parts of a proc (when newly created).
 */
static int
proc_init(void *mem, int size, int flags)
{
	struct proc *p;

	p = (struct proc *)mem;
	mtx_init(&p->p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK | MTX_NEW);
	mtx_init(&p->p_slock, "process slock", NULL, MTX_SPIN | MTX_NEW);
	mtx_init(&p->p_statmtx, "pstatl", NULL, MTX_SPIN | MTX_NEW);
	mtx_init(&p->p_itimmtx, "pitiml", NULL, MTX_SPIN | MTX_NEW);
	mtx_init(&p->p_profmtx, "pprofl", NULL, MTX_SPIN | MTX_NEW);
	cv_init(&p->p_pwait, "ppwait");
	TAILQ_INIT(&p->p_threads);	     /* all threads in proc */
	EVENTHANDLER_DIRECT_INVOKE(process_init, p);
	p->p_stats = pstats_alloc();
	p->p_pgrp = NULL;
	TAILQ_INIT(&p->p_kqtim_stop);
	return (0);
}

/*
 * UMA should ensure that this function is never called.
 * Freeing a proc structure would violate type stability.
 */
static void
proc_fini(void *mem, int size)
{
#ifdef notnow
	struct proc *p;

	p = (struct proc *)mem;
	EVENTHANDLER_DIRECT_INVOKE(process_fini, p);
	pstats_free(p->p_stats);
	thread_free(FIRST_THREAD_IN_PROC(p));
	mtx_destroy(&p->p_mtx);
	if (p->p_ksi != NULL)
		ksiginfo_free(p->p_ksi);
#else
	panic("proc reclaimed");
#endif
}

static int
pgrp_init(void *mem, int size, int flags)
{
	struct pgrp *pg;

	pg = mem;
	mtx_init(&pg->pg_mtx, "process group", NULL, MTX_DEF | MTX_DUPOK);
	sx_init(&pg->pg_killsx, "killpg racer");
	return (0);
}

/*
 * PID space management.
 *
 * These bitmaps are used by fork_findpid.
 */
bitstr_t bit_decl(proc_id_pidmap, PID_MAX);
bitstr_t bit_decl(proc_id_grpidmap, PID_MAX);
bitstr_t bit_decl(proc_id_sessidmap, PID_MAX);
bitstr_t bit_decl(proc_id_reapmap, PID_MAX);

static bitstr_t *proc_id_array[] = {
	proc_id_pidmap,
	proc_id_grpidmap,
	proc_id_sessidmap,
	proc_id_reapmap,
};

void
proc_id_set(int type, pid_t id)
{

	KASSERT(type >= 0 && type < nitems(proc_id_array),
	    ("invalid type %d\n", type));
	mtx_lock(&procid_lock);
	KASSERT(bit_test(proc_id_array[type], id) == 0,
	    ("bit %d already set in %d\n", id, type));
	bit_set(proc_id_array[type], id);
	mtx_unlock(&procid_lock);
}

void
proc_id_set_cond(int type, pid_t id)
{

	KASSERT(type >= 0 && type < nitems(proc_id_array),
	    ("invalid type %d\n", type));
	if (bit_test(proc_id_array[type], id))
		return;
	mtx_lock(&procid_lock);
	bit_set(proc_id_array[type], id);
	mtx_unlock(&procid_lock);
}

void
proc_id_clear(int type, pid_t id)
{

	KASSERT(type >= 0 && type < nitems(proc_id_array),
	    ("invalid type %d\n", type));
	mtx_lock(&procid_lock);
	KASSERT(bit_test(proc_id_array[type], id) != 0,
	    ("bit %d not set in %d\n", id, type));
	bit_clear(proc_id_array[type], id);
	mtx_unlock(&procid_lock);
}

/*
 * Is p an inferior of the current process?
 */
int
inferior(struct proc *p)
{

	sx_assert(&proctree_lock, SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	for (; p != curproc; p = proc_realparent(p)) {
		if (p->p_pid == 0)
			return (0);
	}
	return (1);
}

/*
 * Shared lock all the pid hash lists.
 */
void
pidhash_slockall(void)
{
	u_long i;

	for (i = 0; i < pidhashlock + 1; i++)
		sx_slock(&pidhashtbl_lock[i]);
}

/*
 * Shared unlock all the pid hash lists.
 */
void
pidhash_sunlockall(void)
{
	u_long i;

	for (i = 0; i < pidhashlock + 1; i++)
		sx_sunlock(&pidhashtbl_lock[i]);
}

/*
 * Similar to pfind(), this function locate a process by number.
 */
struct proc *
pfind_any_locked(pid_t pid)
{
	struct proc *p;

	sx_assert(PIDHASHLOCK(pid), SX_LOCKED);
	LIST_FOREACH(p, PIDHASH(pid), p_hash) {
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NEW) {
				PROC_UNLOCK(p);
				p = NULL;
			}
			break;
		}
	}
	return (p);
}

/*
 * Locate a process by number.
 *
 * By not returning processes in the PRS_NEW state, we allow callers to avoid
 * testing for that condition to avoid dereferencing p_ucred, et al.
 */
static __always_inline struct proc *
_pfind(pid_t pid, bool zombie)
{
	struct proc *p;

	p = curproc;
	if (p->p_pid == pid) {
		PROC_LOCK(p);
		return (p);
	}
	sx_slock(PIDHASHLOCK(pid));
	LIST_FOREACH(p, PIDHASH(pid), p_hash) {
		if (p->p_pid == pid) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NEW ||
			    (!zombie && p->p_state == PRS_ZOMBIE)) {
				PROC_UNLOCK(p);
				p = NULL;
			}
			break;
		}
	}
	sx_sunlock(PIDHASHLOCK(pid));
	return (p);
}

struct proc *
pfind(pid_t pid)
{

	return (_pfind(pid, false));
}

/*
 * Same as pfind but allow zombies.
 */
struct proc *
pfind_any(pid_t pid)
{

	return (_pfind(pid, true));
}

/*
 * Locate a process group by number.
 * The caller must hold proctree_lock.
 */
struct pgrp *
pgfind(pid_t pgid)
{
	struct pgrp *pgrp;

	sx_assert(&proctree_lock, SX_LOCKED);

	LIST_FOREACH(pgrp, PGRPHASH(pgid), pg_hash) {
		if (pgrp->pg_id == pgid) {
			PGRP_LOCK(pgrp);
			return (pgrp);
		}
	}
	return (NULL);
}

/*
 * Locate process and do additional manipulations, depending on flags.
 */
int
pget(pid_t pid, int flags, struct proc **pp)
{
	struct proc *p;
	struct thread *td1;
	int error;

	p = curproc;
	if (p->p_pid == pid) {
		PROC_LOCK(p);
	} else {
		p = NULL;
		if (pid <= PID_MAX) {
			if ((flags & PGET_NOTWEXIT) == 0)
				p = pfind_any(pid);
			else
				p = pfind(pid);
		} else if ((flags & PGET_NOTID) == 0) {
			td1 = tdfind(pid, -1);
			if (td1 != NULL)
				p = td1->td_proc;
		}
		if (p == NULL)
			return (ESRCH);
		if ((flags & PGET_CANSEE) != 0) {
			error = p_cansee(curthread, p);
			if (error != 0)
				goto errout;
		}
	}
	if ((flags & PGET_CANDEBUG) != 0) {
		error = p_candebug(curthread, p);
		if (error != 0)
			goto errout;
	}
	if ((flags & PGET_ISCURRENT) != 0 && curproc != p) {
		error = EPERM;
		goto errout;
	}
	if ((flags & PGET_NOTWEXIT) != 0 && (p->p_flag & P_WEXIT) != 0) {
		error = ESRCH;
		goto errout;
	}
	if ((flags & PGET_NOTINEXEC) != 0 && (p->p_flag & P_INEXEC) != 0) {
		/*
		 * XXXRW: Not clear ESRCH is the right error during proc
		 * execve().
		 */
		error = ESRCH;
		goto errout;
	}
	if ((flags & PGET_HOLD) != 0) {
		_PHOLD(p);
		PROC_UNLOCK(p);
	}
	*pp = p;
	return (0);
errout:
	PROC_UNLOCK(p);
	return (error);
}

/*
 * Create a new process group.
 * pgid must be equal to the pid of p.
 * Begin a new session if required.
 */
int
enterpgrp(struct proc *p, pid_t pgid, struct pgrp *pgrp, struct session *sess)
{
	struct pgrp *old_pgrp;

	sx_assert(&proctree_lock, SX_XLOCKED);

	KASSERT(pgrp != NULL, ("enterpgrp: pgrp == NULL"));
	KASSERT(p->p_pid == pgid,
	    ("enterpgrp: new pgrp and pid != pgid"));
	KASSERT(pgfind(pgid) == NULL,
	    ("enterpgrp: pgrp with pgid exists"));
	KASSERT(!SESS_LEADER(p),
	    ("enterpgrp: session leader attempted setpgrp"));

	old_pgrp = p->p_pgrp;
	if (!sx_try_xlock(&old_pgrp->pg_killsx)) {
		sx_xunlock(&proctree_lock);
		sx_xlock(&old_pgrp->pg_killsx);
		sx_xunlock(&old_pgrp->pg_killsx);
		return (ERESTART);
	}
	MPASS(old_pgrp == p->p_pgrp);

	if (sess != NULL) {
		/*
		 * new session
		 */
		mtx_init(&sess->s_mtx, "session", NULL, MTX_DEF);
		PROC_LOCK(p);
		p->p_flag &= ~P_CONTROLT;
		PROC_UNLOCK(p);
		PGRP_LOCK(pgrp);
		sess->s_leader = p;
		sess->s_sid = p->p_pid;
		proc_id_set(PROC_ID_SESSION, p->p_pid);
		refcount_init(&sess->s_count, 1);
		sess->s_ttyvp = NULL;
		sess->s_ttydp = NULL;
		sess->s_ttyp = NULL;
		bcopy(p->p_session->s_login, sess->s_login,
			    sizeof(sess->s_login));
		pgrp->pg_session = sess;
		KASSERT(p == curproc,
		    ("enterpgrp: mksession and p != curproc"));
	} else {
		pgrp->pg_session = p->p_session;
		sess_hold(pgrp->pg_session);
		PGRP_LOCK(pgrp);
	}
	pgrp->pg_id = pgid;
	proc_id_set(PROC_ID_GROUP, p->p_pid);
	LIST_INIT(&pgrp->pg_members);
	pgrp->pg_flags = 0;

	/*
	 * As we have an exclusive lock of proctree_lock,
	 * this should not deadlock.
	 */
	LIST_INSERT_HEAD(PGRPHASH(pgid), pgrp, pg_hash);
	SLIST_INIT(&pgrp->pg_sigiolst);
	PGRP_UNLOCK(pgrp);

	doenterpgrp(p, pgrp);

	sx_xunlock(&old_pgrp->pg_killsx);
	return (0);
}

/*
 * Move p to an existing process group
 */
int
enterthispgrp(struct proc *p, struct pgrp *pgrp)
{
	struct pgrp *old_pgrp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);
	KASSERT(pgrp->pg_session == p->p_session,
	    ("%s: pgrp's session %p, p->p_session %p proc %p\n",
	    __func__, pgrp->pg_session, p->p_session, p));
	KASSERT(pgrp != p->p_pgrp,
	    ("%s: p %p belongs to pgrp %p", __func__, p, pgrp));

	old_pgrp = p->p_pgrp;
	if (!sx_try_xlock(&old_pgrp->pg_killsx)) {
		sx_xunlock(&proctree_lock);
		sx_xlock(&old_pgrp->pg_killsx);
		sx_xunlock(&old_pgrp->pg_killsx);
		return (ERESTART);
	}
	MPASS(old_pgrp == p->p_pgrp);
	if (!sx_try_xlock(&pgrp->pg_killsx)) {
		sx_xunlock(&old_pgrp->pg_killsx);
		sx_xunlock(&proctree_lock);
		sx_xlock(&pgrp->pg_killsx);
		sx_xunlock(&pgrp->pg_killsx);
		return (ERESTART);
	}

	doenterpgrp(p, pgrp);

	sx_xunlock(&pgrp->pg_killsx);
	sx_xunlock(&old_pgrp->pg_killsx);
	return (0);
}

/*
 * If true, any child of q which belongs to group pgrp, qualifies the
 * process group pgrp as not orphaned.
 */
static bool
isjobproc(struct proc *q, struct pgrp *pgrp)
{
	sx_assert(&proctree_lock, SX_LOCKED);

	return (q->p_pgrp != pgrp &&
	    q->p_pgrp->pg_session == pgrp->pg_session);
}

static struct proc *
jobc_reaper(struct proc *p)
{
	struct proc *pp;

	sx_assert(&proctree_lock, SA_LOCKED);

	for (pp = p;;) {
		pp = pp->p_reaper;
		if (pp->p_reaper == pp ||
		    (pp->p_treeflag & P_TREE_GRPEXITED) == 0)
			return (pp);
	}
}

static struct proc *
jobc_parent(struct proc *p, struct proc *p_exiting)
{
	struct proc *pp;

	sx_assert(&proctree_lock, SA_LOCKED);

	pp = proc_realparent(p);
	if (pp->p_pptr == NULL || pp == p_exiting ||
	    (pp->p_treeflag & P_TREE_GRPEXITED) == 0)
		return (pp);
	return (jobc_reaper(pp));
}

int
pgrp_calc_jobc(struct pgrp *pgrp)
{
	struct proc *q;
	int cnt;

#ifdef INVARIANTS
	if (!mtx_owned(&pgrp->pg_mtx))
		sx_assert(&proctree_lock, SA_LOCKED);
#endif

	cnt = 0;
	LIST_FOREACH(q, &pgrp->pg_members, p_pglist) {
		if ((q->p_treeflag & P_TREE_GRPEXITED) != 0 ||
		    q->p_pptr == NULL)
			continue;
		if (isjobproc(jobc_parent(q, NULL), pgrp))
			cnt++;
	}
	return (cnt);
}

/*
 * Move p to a process group
 */
static void
doenterpgrp(struct proc *p, struct pgrp *pgrp)
{
	struct pgrp *savepgrp;
	struct proc *pp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	PGRP_LOCK_ASSERT(p->p_pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(p->p_session, MA_NOTOWNED);

	savepgrp = p->p_pgrp;
	pp = jobc_parent(p, NULL);

	PGRP_LOCK(pgrp);
	PGRP_LOCK(savepgrp);
	if (isjobproc(pp, savepgrp) && pgrp_calc_jobc(savepgrp) == 1)
		orphanpg(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = pgrp;
	PROC_UNLOCK(p);
	LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
	if (isjobproc(pp, pgrp))
		pgrp->pg_flags &= ~PGRP_ORPHANED;
	PGRP_UNLOCK(savepgrp);
	PGRP_UNLOCK(pgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
}

/*
 * remove process from process group
 */
int
leavepgrp(struct proc *p)
{
	struct pgrp *savepgrp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	savepgrp = p->p_pgrp;
	PGRP_LOCK(savepgrp);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_pglist);
	p->p_pgrp = NULL;
	PROC_UNLOCK(p);
	PGRP_UNLOCK(savepgrp);
	if (LIST_EMPTY(&savepgrp->pg_members))
		pgdelete(savepgrp);
	return (0);
}

/*
 * delete a process group
 */
static void
pgdelete(struct pgrp *pgrp)
{
	struct session *savesess;
	struct tty *tp;

	sx_assert(&proctree_lock, SX_XLOCKED);
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pgid.  The proctree lock ensures that
	 * new sigio structures will not be added after this point.
	 */
	funsetownlst(&pgrp->pg_sigiolst);

	PGRP_LOCK(pgrp);
	tp = pgrp->pg_session->s_ttyp;
	LIST_REMOVE(pgrp, pg_hash);
	savesess = pgrp->pg_session;
	PGRP_UNLOCK(pgrp);

	/* Remove the reference to the pgrp before deallocating it. */
	if (tp != NULL) {
		tty_lock(tp);
		tty_rel_pgrp(tp, pgrp);
	}

	proc_id_clear(PROC_ID_GROUP, pgrp->pg_id);
	uma_zfree(pgrp_zone, pgrp);
	sess_release(savesess);
}


static void
fixjobc_kill(struct proc *p)
{
	struct proc *q;
	struct pgrp *pgrp;

	sx_assert(&proctree_lock, SX_LOCKED);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	pgrp = p->p_pgrp;
	PGRP_LOCK_ASSERT(pgrp, MA_NOTOWNED);
	SESS_LOCK_ASSERT(pgrp->pg_session, MA_NOTOWNED);

	/*
	 * p no longer affects process group orphanage for children.
	 * It is marked by the flag because p is only physically
	 * removed from its process group on wait(2).
	 */
	MPASS((p->p_treeflag & P_TREE_GRPEXITED) == 0);
	p->p_treeflag |= P_TREE_GRPEXITED;

	/*
	 * Check if exiting p orphans its own group.
	 */
	pgrp = p->p_pgrp;
	if (isjobproc(jobc_parent(p, NULL), pgrp)) {
		PGRP_LOCK(pgrp);
		if (pgrp_calc_jobc(pgrp) == 0)
			orphanpg(pgrp);
		PGRP_UNLOCK(pgrp);
	}

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups after reparenting to reaper.
	 */
	LIST_FOREACH(q, &p->p_children, p_sibling) {
		pgrp = q->p_pgrp;
		PGRP_LOCK(pgrp);
		if (pgrp_calc_jobc(pgrp) == 0) {
			/*
			 * We want to handle exactly the children that
			 * has p as realparent.  Then, when calculating
			 * jobc_parent for children, we should ignore
			 * P_TREE_GRPEXITED flag already set on p.
			 */
			if (jobc_parent(q, p) == p && isjobproc(p, pgrp))
				orphanpg(pgrp);
		} else
			pgrp->pg_flags &= ~PGRP_ORPHANED;
		PGRP_UNLOCK(pgrp);
	}
	LIST_FOREACH(q, &p->p_orphans, p_orphan) {
		pgrp = q->p_pgrp;
		PGRP_LOCK(pgrp);
		if (pgrp_calc_jobc(pgrp) == 0) {
			if (isjobproc(p, pgrp))
				orphanpg(pgrp);
		} else
			pgrp->pg_flags &= ~PGRP_ORPHANED;
		PGRP_UNLOCK(pgrp);
	}
}

void
killjobc(void)
{
	struct session *sp;
	struct tty *tp;
	struct proc *p;
	struct vnode *ttyvp;

	p = curproc;
	MPASS(p->p_flag & P_WEXIT);
	sx_assert(&proctree_lock, SX_LOCKED);

	if (SESS_LEADER(p)) {
		sp = p->p_session;

		/*
		 * s_ttyp is not zero'd; we use this to indicate that
		 * the session once had a controlling terminal. (for
		 * logging and informational purposes)
		 */
		SESS_LOCK(sp);
		ttyvp = sp->s_ttyvp;
		tp = sp->s_ttyp;
		sp->s_ttyvp = NULL;
		sp->s_ttydp = NULL;
		sp->s_leader = NULL;
		SESS_UNLOCK(sp);

		/*
		 * Signal foreground pgrp and revoke access to
		 * controlling terminal if it has not been revoked
		 * already.
		 *
		 * Because the TTY may have been revoked in the mean
		 * time and could already have a new session associated
		 * with it, make sure we don't send a SIGHUP to a
		 * foreground process group that does not belong to this
		 * session.
		 */

		if (tp != NULL) {
			tty_lock(tp);
			if (tp->t_session == sp)
				tty_signal_pgrp(tp, SIGHUP);
			tty_unlock(tp);
		}

		if (ttyvp != NULL) {
			sx_xunlock(&proctree_lock);
			if (vn_lock(ttyvp, LK_EXCLUSIVE) == 0) {
				VOP_REVOKE(ttyvp, REVOKEALL);
				VOP_UNLOCK(ttyvp);
			}
			devfs_ctty_unref(ttyvp);
			sx_xlock(&proctree_lock);
		}
	}
	fixjobc_kill(p);
}

/*
 * A process group has become orphaned, mark it as such for signal
 * delivery code.  If there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(struct pgrp *pg)
{
	struct proc *p;

	PGRP_LOCK_ASSERT(pg, MA_OWNED);

	pg->pg_flags |= PGRP_ORPHANED;

	LIST_FOREACH(p, &pg->pg_members, p_pglist) {
		PROC_LOCK(p);
		if (P_SHOULDSTOP(p) == P_STOPPED_SIG) {
			PROC_UNLOCK(p);
			LIST_FOREACH(p, &pg->pg_members, p_pglist) {
				PROC_LOCK(p);
				kern_psignal(p, SIGHUP);
				kern_psignal(p, SIGCONT);
				PROC_UNLOCK(p);
			}
			return;
		}
		PROC_UNLOCK(p);
	}
}

void
sess_hold(struct session *s)
{

	refcount_acquire(&s->s_count);
}

void
sess_release(struct session *s)
{

	if (refcount_release(&s->s_count)) {
		if (s->s_ttyp != NULL) {
			tty_lock(s->s_ttyp);
			tty_rel_sess(s->s_ttyp, s);
		}
		proc_id_clear(PROC_ID_SESSION, s->s_sid);
		mtx_destroy(&s->s_mtx);
		free(s, M_SESSION);
	}
}

#ifdef DDB

static void
db_print_pgrp_one(struct pgrp *pgrp, struct proc *p)
{
	db_printf(
	    "    pid %d at %p pr %d pgrp %p e %d jc %d\n",
	    p->p_pid, p, p->p_pptr == NULL ? -1 : p->p_pptr->p_pid,
	    p->p_pgrp, (p->p_treeflag & P_TREE_GRPEXITED) != 0,
	    p->p_pptr == NULL ? 0 : isjobproc(p->p_pptr, pgrp));
}

DB_SHOW_COMMAND_FLAGS(pgrpdump, pgrpdump, DB_CMD_MEMSAFE)
{
	struct pgrp *pgrp;
	struct proc *p;
	int i;

	for (i = 0; i <= pgrphash; i++) {
		if (!LIST_EMPTY(&pgrphashtbl[i])) {
			db_printf("indx %d\n", i);
			LIST_FOREACH(pgrp, &pgrphashtbl[i], pg_hash) {
				db_printf(
			"  pgrp %p, pgid %d, sess %p, sesscnt %d, mem %p\n",
				    pgrp, (int)pgrp->pg_id, pgrp->pg_session,
				    pgrp->pg_session->s_count,
				    LIST_FIRST(&pgrp->pg_members));
				LIST_FOREACH(p, &pgrp->pg_members, p_pglist)
					db_print_pgrp_one(pgrp, p);
			}
		}
	}
}
#endif /* DDB */

/*
 * Calculate the kinfo_proc members which contain process-wide
 * informations.
 * Must be called with the target process locked.
 */
static void
fill_kinfo_aggregate(struct proc *p, struct kinfo_proc *kp)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	kp->ki_estcpu = 0;
	kp->ki_pctcpu = 0;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		kp->ki_pctcpu += sched_pctcpu(td);
		kp->ki_estcpu += sched_estcpu(td);
		thread_unlock(td);
	}
}

/*
 * Fill in any information that is common to all threads in the process.
 * Must be called with the target process locked.
 */
static void
fill_kinfo_proc_only(struct proc *p, struct kinfo_proc *kp)
{
	struct thread *td0;
	struct ucred *cred;
	struct sigacts *ps;
	struct timeval boottime;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	kp->ki_structsize = sizeof(*kp);
	kp->ki_paddr = p;
	kp->ki_addr =/* p->p_addr; */0; /* XXX */
	kp->ki_args = p->p_args;
	kp->ki_textvp = p->p_textvp;
#ifdef KTRACE
	kp->ki_tracep = ktr_get_tracevp(p, false);
	kp->ki_traceflag = p->p_traceflag;
#endif
	kp->ki_fd = p->p_fd;
	kp->ki_pd = p->p_pd;
	kp->ki_vmspace = p->p_vmspace;
	kp->ki_flag = p->p_flag;
	kp->ki_flag2 = p->p_flag2;
	cred = p->p_ucred;
	if (cred) {
		kp->ki_uid = cred->cr_uid;
		kp->ki_ruid = cred->cr_ruid;
		kp->ki_svuid = cred->cr_svuid;
		kp->ki_cr_flags = 0;
		if (cred->cr_flags & CRED_FLAG_CAPMODE)
			kp->ki_cr_flags |= KI_CRF_CAPABILITY_MODE;
		/* XXX bde doesn't like KI_NGROUPS */
		if (cred->cr_ngroups > KI_NGROUPS) {
			kp->ki_ngroups = KI_NGROUPS;
			kp->ki_cr_flags |= KI_CRF_GRP_OVERFLOW;
		} else
			kp->ki_ngroups = cred->cr_ngroups;
		bcopy(cred->cr_groups, kp->ki_groups,
		    kp->ki_ngroups * sizeof(gid_t));
		kp->ki_rgid = cred->cr_rgid;
		kp->ki_svgid = cred->cr_svgid;
		/* If jailed(cred), emulate the old P_JAILED flag. */
		if (jailed(cred)) {
			kp->ki_flag |= P_JAILED;
			/* If inside the jail, use 0 as a jail ID. */
			if (cred->cr_prison != curthread->td_ucred->cr_prison)
				kp->ki_jid = cred->cr_prison->pr_id;
		}
		strlcpy(kp->ki_loginclass, cred->cr_loginclass->lc_name,
		    sizeof(kp->ki_loginclass));
	}
	ps = p->p_sigacts;
	if (ps) {
		mtx_lock(&ps->ps_mtx);
		kp->ki_sigignore = ps->ps_sigignore;
		kp->ki_sigcatch = ps->ps_sigcatch;
		mtx_unlock(&ps->ps_mtx);
	}
	if (p->p_state != PRS_NEW &&
	    p->p_state != PRS_ZOMBIE &&
	    p->p_vmspace != NULL) {
		struct vmspace *vm = p->p_vmspace;

		kp->ki_size = vm->vm_map.size;
		kp->ki_rssize = vmspace_resident_count(vm); /*XXX*/
		FOREACH_THREAD_IN_PROC(p, td0)
			kp->ki_rssize += td0->td_kstack_pages;
		kp->ki_swrss = vm->vm_swrss;
		kp->ki_tsize = vm->vm_tsize;
		kp->ki_dsize = vm->vm_dsize;
		kp->ki_ssize = vm->vm_ssize;
	} else if (p->p_state == PRS_ZOMBIE)
		kp->ki_stat = SZOMB;
	kp->ki_sflag = PS_INMEM;
	/* Calculate legacy swtime as seconds since 'swtick'. */
	kp->ki_swtime = (ticks - p->p_swtick) / hz;
	kp->ki_pid = p->p_pid;
	kp->ki_nice = p->p_nice;
	kp->ki_fibnum = p->p_fibnum;
	kp->ki_start = p->p_stats->p_start;
	getboottime(&boottime);
	timevaladd(&kp->ki_start, &boottime);
	PROC_STATLOCK(p);
	rufetch(p, &kp->ki_rusage);
	kp->ki_runtime = cputick2usec(p->p_rux.rux_runtime);
	calcru(p, &kp->ki_rusage.ru_utime, &kp->ki_rusage.ru_stime);
	PROC_STATUNLOCK(p);
	calccru(p, &kp->ki_childutime, &kp->ki_childstime);
	/* Some callers want child times in a single value. */
	kp->ki_childtime = kp->ki_childstime;
	timevaladd(&kp->ki_childtime, &kp->ki_childutime);

	FOREACH_THREAD_IN_PROC(p, td0)
		kp->ki_cow += td0->td_cow;

	if (p->p_comm[0] != '\0')
		strlcpy(kp->ki_comm, p->p_comm, sizeof(kp->ki_comm));
	if (p->p_sysent && p->p_sysent->sv_name != NULL &&
	    p->p_sysent->sv_name[0] != '\0')
		strlcpy(kp->ki_emul, p->p_sysent->sv_name, sizeof(kp->ki_emul));
	kp->ki_siglist = p->p_siglist;
	kp->ki_xstat = KW_EXITCODE(p->p_xexit, p->p_xsig);
	kp->ki_acflag = p->p_acflag;
	kp->ki_lock = p->p_lock;
	if (p->p_pptr) {
		kp->ki_ppid = p->p_oppid;
		if (p->p_flag & P_TRACED)
			kp->ki_tracer = p->p_pptr->p_pid;
	}
}

/*
 * Fill job-related process information.
 */
static void
fill_kinfo_proc_pgrp(struct proc *p, struct kinfo_proc *kp)
{
	struct tty *tp;
	struct session *sp;
	struct pgrp *pgrp;

	sx_assert(&proctree_lock, SA_LOCKED);
	PROC_LOCK_ASSERT(p, MA_OWNED);

	pgrp = p->p_pgrp;
	if (pgrp == NULL)
		return;

	kp->ki_pgid = pgrp->pg_id;
	kp->ki_jobc = pgrp_calc_jobc(pgrp);

	sp = pgrp->pg_session;
	tp = NULL;

	if (sp != NULL) {
		kp->ki_sid = sp->s_sid;
		SESS_LOCK(sp);
		strlcpy(kp->ki_login, sp->s_login, sizeof(kp->ki_login));
		if (sp->s_ttyvp)
			kp->ki_kiflag |= KI_CTTY;
		if (SESS_LEADER(p))
			kp->ki_kiflag |= KI_SLEADER;
		tp = sp->s_ttyp;
		SESS_UNLOCK(sp);
	}

	if ((p->p_flag & P_CONTROLT) && tp != NULL) {
		kp->ki_tdev = tty_udev(tp);
		kp->ki_tdev_freebsd11 = kp->ki_tdev; /* truncate */
		kp->ki_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		if (tp->t_session)
			kp->ki_tsid = tp->t_session->s_sid;
	} else {
		kp->ki_tdev = NODEV;
		kp->ki_tdev_freebsd11 = kp->ki_tdev; /* truncate */
	}
}

/*
 * Fill in information that is thread specific.  Must be called with
 * target process locked.  If 'preferthread' is set, overwrite certain
 * process-related fields that are maintained for both threads and
 * processes.
 */
static void
fill_kinfo_thread(struct thread *td, struct kinfo_proc *kp, int preferthread)
{
	struct proc *p;

	p = td->td_proc;
	kp->ki_tdaddr = td;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (preferthread)
		PROC_STATLOCK(p);
	thread_lock(td);
	if (td->td_wmesg != NULL)
		strlcpy(kp->ki_wmesg, td->td_wmesg, sizeof(kp->ki_wmesg));
	else
		bzero(kp->ki_wmesg, sizeof(kp->ki_wmesg));
	if (strlcpy(kp->ki_tdname, td->td_name, sizeof(kp->ki_tdname)) >=
	    sizeof(kp->ki_tdname)) {
		strlcpy(kp->ki_moretdname,
		    td->td_name + sizeof(kp->ki_tdname) - 1,
		    sizeof(kp->ki_moretdname));
	} else {
		bzero(kp->ki_moretdname, sizeof(kp->ki_moretdname));
	}
	if (TD_ON_LOCK(td)) {
		kp->ki_kiflag |= KI_LOCKBLOCK;
		strlcpy(kp->ki_lockname, td->td_lockname,
		    sizeof(kp->ki_lockname));
	} else {
		kp->ki_kiflag &= ~KI_LOCKBLOCK;
		bzero(kp->ki_lockname, sizeof(kp->ki_lockname));
	}

	if (p->p_state == PRS_NORMAL) { /* approximate. */
		if (TD_ON_RUNQ(td) ||
		    TD_CAN_RUN(td) ||
		    TD_IS_RUNNING(td)) {
			kp->ki_stat = SRUN;
		} else if (P_SHOULDSTOP(p)) {
			kp->ki_stat = SSTOP;
		} else if (TD_IS_SLEEPING(td)) {
			kp->ki_stat = SSLEEP;
		} else if (TD_ON_LOCK(td)) {
			kp->ki_stat = SLOCK;
		} else {
			kp->ki_stat = SWAIT;
		}
	} else if (p->p_state == PRS_ZOMBIE) {
		kp->ki_stat = SZOMB;
	} else {
		kp->ki_stat = SIDL;
	}

	/* Things in the thread */
	kp->ki_wchan = td->td_wchan;
	kp->ki_pri.pri_level = td->td_priority;
	kp->ki_pri.pri_native = td->td_base_pri;

	/*
	 * Note: legacy fields; clamp at the old NOCPU value and/or
	 * the maximum u_char CPU value.
	 */
	if (td->td_lastcpu == NOCPU)
		kp->ki_lastcpu_old = NOCPU_OLD;
	else if (td->td_lastcpu > MAXCPU_OLD)
		kp->ki_lastcpu_old = MAXCPU_OLD;
	else
		kp->ki_lastcpu_old = td->td_lastcpu;

	if (td->td_oncpu == NOCPU)
		kp->ki_oncpu_old = NOCPU_OLD;
	else if (td->td_oncpu > MAXCPU_OLD)
		kp->ki_oncpu_old = MAXCPU_OLD;
	else
		kp->ki_oncpu_old = td->td_oncpu;

	kp->ki_lastcpu = td->td_lastcpu;
	kp->ki_oncpu = td->td_oncpu;
	kp->ki_tdflags = td->td_flags;
	kp->ki_tid = td->td_tid;
	kp->ki_numthreads = p->p_numthreads;
	kp->ki_pcb = td->td_pcb;
	kp->ki_kstack = (void *)td->td_kstack;
	kp->ki_slptime = (ticks - td->td_slptick) / hz;
	kp->ki_pri.pri_class = td->td_pri_class;
	kp->ki_pri.pri_user = td->td_user_pri;

	if (preferthread) {
		rufetchtd(td, &kp->ki_rusage);
		kp->ki_runtime = cputick2usec(td->td_rux.rux_runtime);
		kp->ki_pctcpu = sched_pctcpu(td);
		kp->ki_estcpu = sched_estcpu(td);
		kp->ki_cow = td->td_cow;
	}

	/* We can't get this anymore but ps etc never used it anyway. */
	kp->ki_rqindex = 0;

	if (preferthread)
		kp->ki_siglist = td->td_siglist;
	kp->ki_sigmask = td->td_sigmask;
	thread_unlock(td);
	if (preferthread)
		PROC_STATUNLOCK(p);
}

/*
 * Fill in a kinfo_proc structure for the specified process.
 * Must be called with the target process locked.
 */
void
fill_kinfo_proc(struct proc *p, struct kinfo_proc *kp)
{
	MPASS(FIRST_THREAD_IN_PROC(p) != NULL);

	bzero(kp, sizeof(*kp));

	fill_kinfo_proc_pgrp(p,kp);
	fill_kinfo_proc_only(p, kp);
	fill_kinfo_thread(FIRST_THREAD_IN_PROC(p), kp, 0);
	fill_kinfo_aggregate(p, kp);
}

struct pstats *
pstats_alloc(void)
{

	return (malloc(sizeof(struct pstats), M_SUBPROC, M_ZERO|M_WAITOK));
}

/*
 * Copy parts of p_stats; zero the rest of p_stats (statistics).
 */
void
pstats_fork(struct pstats *src, struct pstats *dst)
{

	bzero(&dst->pstat_startzero,
	    __rangeof(struct pstats, pstat_startzero, pstat_endzero));
	bcopy(&src->pstat_startcopy, &dst->pstat_startcopy,
	    __rangeof(struct pstats, pstat_startcopy, pstat_endcopy));
}

void
pstats_free(struct pstats *ps)
{

	free(ps, M_SUBPROC);
}

#ifdef COMPAT_FREEBSD32

/*
 * This function is typically used to copy out the kernel address, so
 * it can be replaced by assignment of zero.
 */
static inline uint32_t
ptr32_trim(const void *ptr)
{
	uintptr_t uptr;

	uptr = (uintptr_t)ptr;
	return ((uptr > UINT_MAX) ? 0 : uptr);
}

#define PTRTRIM_CP(src,dst,fld) \
	do { (dst).fld = ptr32_trim((src).fld); } while (0)

static void
freebsd32_kinfo_proc_out(const struct kinfo_proc *ki, struct kinfo_proc32 *ki32)
{
	int i;

	bzero(ki32, sizeof(struct kinfo_proc32));
	ki32->ki_structsize = sizeof(struct kinfo_proc32);
	CP(*ki, *ki32, ki_layout);
	PTRTRIM_CP(*ki, *ki32, ki_args);
	PTRTRIM_CP(*ki, *ki32, ki_paddr);
	PTRTRIM_CP(*ki, *ki32, ki_addr);
	PTRTRIM_CP(*ki, *ki32, ki_tracep);
	PTRTRIM_CP(*ki, *ki32, ki_textvp);
	PTRTRIM_CP(*ki, *ki32, ki_fd);
	PTRTRIM_CP(*ki, *ki32, ki_vmspace);
	PTRTRIM_CP(*ki, *ki32, ki_wchan);
	CP(*ki, *ki32, ki_pid);
	CP(*ki, *ki32, ki_ppid);
	CP(*ki, *ki32, ki_pgid);
	CP(*ki, *ki32, ki_tpgid);
	CP(*ki, *ki32, ki_sid);
	CP(*ki, *ki32, ki_tsid);
	CP(*ki, *ki32, ki_jobc);
	CP(*ki, *ki32, ki_tdev);
	CP(*ki, *ki32, ki_tdev_freebsd11);
	CP(*ki, *ki32, ki_siglist);
	CP(*ki, *ki32, ki_sigmask);
	CP(*ki, *ki32, ki_sigignore);
	CP(*ki, *ki32, ki_sigcatch);
	CP(*ki, *ki32, ki_uid);
	CP(*ki, *ki32, ki_ruid);
	CP(*ki, *ki32, ki_svuid);
	CP(*ki, *ki32, ki_rgid);
	CP(*ki, *ki32, ki_svgid);
	CP(*ki, *ki32, ki_ngroups);
	for (i = 0; i < KI_NGROUPS; i++)
		CP(*ki, *ki32, ki_groups[i]);
	CP(*ki, *ki32, ki_size);
	CP(*ki, *ki32, ki_rssize);
	CP(*ki, *ki32, ki_swrss);
	CP(*ki, *ki32, ki_tsize);
	CP(*ki, *ki32, ki_dsize);
	CP(*ki, *ki32, ki_ssize);
	CP(*ki, *ki32, ki_xstat);
	CP(*ki, *ki32, ki_acflag);
	CP(*ki, *ki32, ki_pctcpu);
	CP(*ki, *ki32, ki_estcpu);
	CP(*ki, *ki32, ki_slptime);
	CP(*ki, *ki32, ki_swtime);
	CP(*ki, *ki32, ki_cow);
	CP(*ki, *ki32, ki_runtime);
	TV_CP(*ki, *ki32, ki_start);
	TV_CP(*ki, *ki32, ki_childtime);
	CP(*ki, *ki32, ki_flag);
	CP(*ki, *ki32, ki_kiflag);
	CP(*ki, *ki32, ki_traceflag);
	CP(*ki, *ki32, ki_stat);
	CP(*ki, *ki32, ki_nice);
	CP(*ki, *ki32, ki_lock);
	CP(*ki, *ki32, ki_rqindex);
	CP(*ki, *ki32, ki_oncpu);
	CP(*ki, *ki32, ki_lastcpu);

	/* XXX TODO: wrap cpu value as appropriate */
	CP(*ki, *ki32, ki_oncpu_old);
	CP(*ki, *ki32, ki_lastcpu_old);

	bcopy(ki->ki_tdname, ki32->ki_tdname, TDNAMLEN + 1);
	bcopy(ki->ki_wmesg, ki32->ki_wmesg, WMESGLEN + 1);
	bcopy(ki->ki_login, ki32->ki_login, LOGNAMELEN + 1);
	bcopy(ki->ki_lockname, ki32->ki_lockname, LOCKNAMELEN + 1);
	bcopy(ki->ki_comm, ki32->ki_comm, COMMLEN + 1);
	bcopy(ki->ki_emul, ki32->ki_emul, KI_EMULNAMELEN + 1);
	bcopy(ki->ki_loginclass, ki32->ki_loginclass, LOGINCLASSLEN + 1);
	bcopy(ki->ki_moretdname, ki32->ki_moretdname, MAXCOMLEN - TDNAMLEN + 1);
	CP(*ki, *ki32, ki_tracer);
	CP(*ki, *ki32, ki_flag2);
	CP(*ki, *ki32, ki_fibnum);
	CP(*ki, *ki32, ki_cr_flags);
	CP(*ki, *ki32, ki_jid);
	CP(*ki, *ki32, ki_numthreads);
	CP(*ki, *ki32, ki_tid);
	CP(*ki, *ki32, ki_pri);
	freebsd32_rusage_out(&ki->ki_rusage, &ki32->ki_rusage);
	freebsd32_rusage_out(&ki->ki_rusage_ch, &ki32->ki_rusage_ch);
	PTRTRIM_CP(*ki, *ki32, ki_pcb);
	PTRTRIM_CP(*ki, *ki32, ki_kstack);
	PTRTRIM_CP(*ki, *ki32, ki_udata);
	PTRTRIM_CP(*ki, *ki32, ki_tdaddr);
	CP(*ki, *ki32, ki_sflag);
	CP(*ki, *ki32, ki_tdflags);
}
#endif

static ssize_t
kern_proc_out_size(struct proc *p, int flags)
{
	ssize_t size = 0;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	if ((flags & KERN_PROC_NOTHREADS) != 0) {
#ifdef COMPAT_FREEBSD32
		if ((flags & KERN_PROC_MASK32) != 0) {
			size += sizeof(struct kinfo_proc32);
		} else
#endif
			size += sizeof(struct kinfo_proc);
	} else {
#ifdef COMPAT_FREEBSD32
		if ((flags & KERN_PROC_MASK32) != 0)
			size += sizeof(struct kinfo_proc32) * p->p_numthreads;
		else
#endif
			size += sizeof(struct kinfo_proc) * p->p_numthreads;
	}
	PROC_UNLOCK(p);
	return (size);
}

int
kern_proc_out(struct proc *p, struct sbuf *sb, int flags)
{
	struct thread *td;
	struct kinfo_proc ki;
#ifdef COMPAT_FREEBSD32
	struct kinfo_proc32 ki32;
#endif
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS(FIRST_THREAD_IN_PROC(p) != NULL);

	error = 0;
	fill_kinfo_proc(p, &ki);
	if ((flags & KERN_PROC_NOTHREADS) != 0) {
#ifdef COMPAT_FREEBSD32
		if ((flags & KERN_PROC_MASK32) != 0) {
			freebsd32_kinfo_proc_out(&ki, &ki32);
			if (sbuf_bcat(sb, &ki32, sizeof(ki32)) != 0)
				error = ENOMEM;
		} else
#endif
			if (sbuf_bcat(sb, &ki, sizeof(ki)) != 0)
				error = ENOMEM;
	} else {
		FOREACH_THREAD_IN_PROC(p, td) {
			fill_kinfo_thread(td, &ki, 1);
#ifdef COMPAT_FREEBSD32
			if ((flags & KERN_PROC_MASK32) != 0) {
				freebsd32_kinfo_proc_out(&ki, &ki32);
				if (sbuf_bcat(sb, &ki32, sizeof(ki32)) != 0)
					error = ENOMEM;
			} else
#endif
				if (sbuf_bcat(sb, &ki, sizeof(ki)) != 0)
					error = ENOMEM;
			if (error != 0)
				break;
		}
	}
	PROC_UNLOCK(p);
	return (error);
}

static int
sysctl_out_proc(struct proc *p, struct sysctl_req *req, int flags)
{
	struct sbuf sb;
	struct kinfo_proc ki;
	int error, error2;

	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, kern_proc_out_size(p, flags)));

	sbuf_new_for_sysctl(&sb, (char *)&ki, sizeof(ki), req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = kern_proc_out(p, &sb, flags);
	error2 = sbuf_finish(&sb);
	sbuf_delete(&sb);
	if (error != 0)
		return (error);
	else if (error2 != 0)
		return (error2);
	return (0);
}

int
proc_iterate(int (*cb)(struct proc *, void *), void *cbarg)
{
	struct proc *p;
	int error, i, j;

	for (i = 0; i < pidhashlock + 1; i++) {
		sx_slock(&proctree_lock);
		sx_slock(&pidhashtbl_lock[i]);
		for (j = i; j <= pidhash; j += pidhashlock + 1) {
			LIST_FOREACH(p, &pidhashtbl[j], p_hash) {
				if (p->p_state == PRS_NEW)
					continue;
				error = cb(p, cbarg);
				PROC_LOCK_ASSERT(p, MA_NOTOWNED);
				if (error != 0) {
					sx_sunlock(&pidhashtbl_lock[i]);
					sx_sunlock(&proctree_lock);
					return (error);
				}
			}
		}
		sx_sunlock(&pidhashtbl_lock[i]);
		sx_sunlock(&proctree_lock);
	}
	return (0);
}

struct kern_proc_out_args {
	struct sysctl_req *req;
	int flags;
	int oid_number;
	int *name;
};

static int
sysctl_kern_proc_iterate(struct proc *p, void *origarg)
{
	struct kern_proc_out_args *arg = origarg;
	int *name = arg->name;
	int oid_number = arg->oid_number;
	int flags = arg->flags;
	struct sysctl_req *req = arg->req;
	int error = 0;

	PROC_LOCK(p);

	KASSERT(p->p_ucred != NULL,
	    ("process credential is NULL for non-NEW proc"));
	/*
	 * Show a user only appropriate processes.
	 */
	if (p_cansee(curthread, p))
		goto skip;
	/*
	 * TODO - make more efficient (see notes below).
	 * do by session.
	 */
	switch (oid_number) {
	case KERN_PROC_GID:
		if (p->p_ucred->cr_gid != (gid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_PGRP:
		/* could do this by traversing pgrp */
		if (p->p_pgrp == NULL ||
		    p->p_pgrp->pg_id != (pid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_RGID:
		if (p->p_ucred->cr_rgid != (gid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_SESSION:
		if (p->p_session == NULL ||
		    p->p_session->s_sid != (pid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_TTY:
		if ((p->p_flag & P_CONTROLT) == 0 ||
		    p->p_session == NULL)
			goto skip;
		/* XXX proctree_lock */
		SESS_LOCK(p->p_session);
		if (p->p_session->s_ttyp == NULL ||
		    tty_udev(p->p_session->s_ttyp) !=
		    (dev_t)name[0]) {
			SESS_UNLOCK(p->p_session);
			goto skip;
		}
		SESS_UNLOCK(p->p_session);
		break;

	case KERN_PROC_UID:
		if (p->p_ucred->cr_uid != (uid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_RUID:
		if (p->p_ucred->cr_ruid != (uid_t)name[0])
			goto skip;
		break;

	case KERN_PROC_PROC:
		break;

	default:
		break;
	}
	error = sysctl_out_proc(p, req, flags);
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	return (error);
skip:
	PROC_UNLOCK(p);
	return (0);
}

static int
sysctl_kern_proc(SYSCTL_HANDLER_ARGS)
{
	struct kern_proc_out_args iterarg;
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	int flags, oid_number;
	int error = 0;

	oid_number = oidp->oid_number;
	if (oid_number != KERN_PROC_ALL &&
	    (oid_number & KERN_PROC_INC_THREAD) == 0)
		flags = KERN_PROC_NOTHREADS;
	else {
		flags = 0;
		oid_number &= ~KERN_PROC_INC_THREAD;
	}
#ifdef COMPAT_FREEBSD32
	if (req->flags & SCTL_MASK32)
		flags |= KERN_PROC_MASK32;
#endif
	if (oid_number == KERN_PROC_PID) {
		if (namelen != 1)
			return (EINVAL);
		error = sysctl_wire_old_buffer(req, 0);
		if (error)
			return (error);
		sx_slock(&proctree_lock);
		error = pget((pid_t)name[0], PGET_CANSEE, &p);
		if (error == 0)
			error = sysctl_out_proc(p, req, flags);
		sx_sunlock(&proctree_lock);
		return (error);
	}

	switch (oid_number) {
	case KERN_PROC_ALL:
		if (namelen != 0)
			return (EINVAL);
		break;
	case KERN_PROC_PROC:
		if (namelen != 0 && namelen != 1)
			return (EINVAL);
		break;
	default:
		if (namelen != 1)
			return (EINVAL);
		break;
	}

	if (req->oldptr == NULL) {
		/* overestimate by 5 procs */
		error = SYSCTL_OUT(req, 0, sizeof (struct kinfo_proc) * 5);
		if (error)
			return (error);
	} else {
		error = sysctl_wire_old_buffer(req, 0);
		if (error != 0)
			return (error);
	}
	iterarg.flags = flags;
	iterarg.oid_number = oid_number;
	iterarg.req = req;
	iterarg.name = name;
	error = proc_iterate(sysctl_kern_proc_iterate, &iterarg);
	return (error);
}

struct pargs *
pargs_alloc(int len)
{
	struct pargs *pa;

	pa = malloc(sizeof(struct pargs) + len, M_PARGS,
		M_WAITOK);
	refcount_init(&pa->ar_ref, 1);
	pa->ar_length = len;
	return (pa);
}

static void
pargs_free(struct pargs *pa)
{

	free(pa, M_PARGS);
}

void
pargs_hold(struct pargs *pa)
{

	if (pa == NULL)
		return;
	refcount_acquire(&pa->ar_ref);
}

void
pargs_drop(struct pargs *pa)
{

	if (pa == NULL)
		return;
	if (refcount_release(&pa->ar_ref))
		pargs_free(pa);
}

static int
proc_read_string(struct thread *td, struct proc *p, const char *sptr, char *buf,
    size_t len)
{
	ssize_t n;

	/*
	 * This may return a short read if the string is shorter than the chunk
	 * and is aligned at the end of the page, and the following page is not
	 * mapped.
	 */
	n = proc_readmem(td, p, (vm_offset_t)sptr, buf, len);
	if (n <= 0)
		return (ENOMEM);
	return (0);
}

#define PROC_AUXV_MAX	256	/* Safety limit on auxv size. */

enum proc_vector_type {
	PROC_ARG,
	PROC_ENV,
	PROC_AUX,
};

#ifdef COMPAT_FREEBSD32
static int
get_proc_vector32(struct thread *td, struct proc *p, char ***proc_vectorp,
    size_t *vsizep, enum proc_vector_type type)
{
	struct freebsd32_ps_strings pss;
	Elf32_Auxinfo aux;
	vm_offset_t vptr, ptr;
	uint32_t *proc_vector32;
	char **proc_vector;
	size_t vsize, size;
	int i, error;

	error = 0;
	if (proc_readmem(td, p, PROC_PS_STRINGS(p), &pss, sizeof(pss)) !=
	    sizeof(pss))
		return (ENOMEM);
	switch (type) {
	case PROC_ARG:
		vptr = (vm_offset_t)PTRIN(pss.ps_argvstr);
		vsize = pss.ps_nargvstr;
		if (vsize > ARG_MAX)
			return (ENOEXEC);
		size = vsize * sizeof(int32_t);
		break;
	case PROC_ENV:
		vptr = (vm_offset_t)PTRIN(pss.ps_envstr);
		vsize = pss.ps_nenvstr;
		if (vsize > ARG_MAX)
			return (ENOEXEC);
		size = vsize * sizeof(int32_t);
		break;
	case PROC_AUX:
		vptr = (vm_offset_t)PTRIN(pss.ps_envstr) +
		    (pss.ps_nenvstr + 1) * sizeof(int32_t);
		if (vptr % 4 != 0)
			return (ENOEXEC);
		for (ptr = vptr, i = 0; i < PROC_AUXV_MAX; i++) {
			if (proc_readmem(td, p, ptr, &aux, sizeof(aux)) !=
			    sizeof(aux))
				return (ENOMEM);
			if (aux.a_type == AT_NULL)
				break;
			ptr += sizeof(aux);
		}
		if (aux.a_type != AT_NULL)
			return (ENOEXEC);
		vsize = i + 1;
		size = vsize * sizeof(aux);
		break;
	default:
		KASSERT(0, ("Wrong proc vector type: %d", type));
		return (EINVAL);
	}
	proc_vector32 = malloc(size, M_TEMP, M_WAITOK);
	if (proc_readmem(td, p, vptr, proc_vector32, size) != size) {
		error = ENOMEM;
		goto done;
	}
	if (type == PROC_AUX) {
		*proc_vectorp = (char **)proc_vector32;
		*vsizep = vsize;
		return (0);
	}
	proc_vector = malloc(vsize * sizeof(char *), M_TEMP, M_WAITOK);
	for (i = 0; i < (int)vsize; i++)
		proc_vector[i] = PTRIN(proc_vector32[i]);
	*proc_vectorp = proc_vector;
	*vsizep = vsize;
done:
	free(proc_vector32, M_TEMP);
	return (error);
}
#endif

static int
get_proc_vector(struct thread *td, struct proc *p, char ***proc_vectorp,
    size_t *vsizep, enum proc_vector_type type)
{
	struct ps_strings pss;
	Elf_Auxinfo aux;
	vm_offset_t vptr, ptr;
	char **proc_vector;
	size_t vsize, size;
	int i;

#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(p, SV_ILP32) != 0)
		return (get_proc_vector32(td, p, proc_vectorp, vsizep, type));
#endif
	if (proc_readmem(td, p, PROC_PS_STRINGS(p), &pss, sizeof(pss)) !=
	    sizeof(pss))
		return (ENOMEM);
	switch (type) {
	case PROC_ARG:
		vptr = (vm_offset_t)pss.ps_argvstr;
		vsize = pss.ps_nargvstr;
		if (vsize > ARG_MAX)
			return (ENOEXEC);
		size = vsize * sizeof(char *);
		break;
	case PROC_ENV:
		vptr = (vm_offset_t)pss.ps_envstr;
		vsize = pss.ps_nenvstr;
		if (vsize > ARG_MAX)
			return (ENOEXEC);
		size = vsize * sizeof(char *);
		break;
	case PROC_AUX:
		/*
		 * The aux array is just above env array on the stack. Check
		 * that the address is naturally aligned.
		 */
		vptr = (vm_offset_t)pss.ps_envstr + (pss.ps_nenvstr + 1)
		    * sizeof(char *);
#if __ELF_WORD_SIZE == 64
		if (vptr % sizeof(uint64_t) != 0)
#else
		if (vptr % sizeof(uint32_t) != 0)
#endif
			return (ENOEXEC);
		/*
		 * We count the array size reading the aux vectors from the
		 * stack until AT_NULL vector is returned.  So (to keep the code
		 * simple) we read the process stack twice: the first time here
		 * to find the size and the second time when copying the vectors
		 * to the allocated proc_vector.
		 */
		for (ptr = vptr, i = 0; i < PROC_AUXV_MAX; i++) {
			if (proc_readmem(td, p, ptr, &aux, sizeof(aux)) !=
			    sizeof(aux))
				return (ENOMEM);
			if (aux.a_type == AT_NULL)
				break;
			ptr += sizeof(aux);
		}
		/*
		 * If the PROC_AUXV_MAX entries are iterated over, and we have
		 * not reached AT_NULL, it is most likely we are reading wrong
		 * data: either the process doesn't have auxv array or data has
		 * been modified. Return the error in this case.
		 */
		if (aux.a_type != AT_NULL)
			return (ENOEXEC);
		vsize = i + 1;
		size = vsize * sizeof(aux);
		break;
	default:
		KASSERT(0, ("Wrong proc vector type: %d", type));
		return (EINVAL); /* In case we are built without INVARIANTS. */
	}
	proc_vector = malloc(size, M_TEMP, M_WAITOK);
	if (proc_readmem(td, p, vptr, proc_vector, size) != size) {
		free(proc_vector, M_TEMP);
		return (ENOMEM);
	}
	*proc_vectorp = proc_vector;
	*vsizep = vsize;

	return (0);
}

#define GET_PS_STRINGS_CHUNK_SZ	256	/* Chunk size (bytes) for ps_strings operations. */

static int
get_ps_strings(struct thread *td, struct proc *p, struct sbuf *sb,
    enum proc_vector_type type)
{
	size_t done, len, nchr, vsize;
	int error, i;
	char **proc_vector, *sptr;
	char pss_string[GET_PS_STRINGS_CHUNK_SZ];

	PROC_ASSERT_HELD(p);

	/*
	 * We are not going to read more than 2 * (PATH_MAX + ARG_MAX) bytes.
	 */
	nchr = 2 * (PATH_MAX + ARG_MAX);

	error = get_proc_vector(td, p, &proc_vector, &vsize, type);
	if (error != 0)
		return (error);
	for (done = 0, i = 0; i < (int)vsize && done < nchr; i++) {
		/*
		 * The program may have scribbled into its argv array, e.g. to
		 * remove some arguments.  If that has happened, break out
		 * before trying to read from NULL.
		 */
		if (proc_vector[i] == NULL)
			break;
		for (sptr = proc_vector[i]; ; sptr += GET_PS_STRINGS_CHUNK_SZ) {
			error = proc_read_string(td, p, sptr, pss_string,
			    sizeof(pss_string));
			if (error != 0)
				goto done;
			len = strnlen(pss_string, GET_PS_STRINGS_CHUNK_SZ);
			if (done + len >= nchr)
				len = nchr - done - 1;
			sbuf_bcat(sb, pss_string, len);
			if (len != GET_PS_STRINGS_CHUNK_SZ)
				break;
			done += GET_PS_STRINGS_CHUNK_SZ;
		}
		sbuf_bcat(sb, "", 1);
		done += len + 1;
	}
done:
	free(proc_vector, M_TEMP);
	return (error);
}

int
proc_getargv(struct thread *td, struct proc *p, struct sbuf *sb)
{

	return (get_ps_strings(curthread, p, sb, PROC_ARG));
}

int
proc_getenvv(struct thread *td, struct proc *p, struct sbuf *sb)
{

	return (get_ps_strings(curthread, p, sb, PROC_ENV));
}

int
proc_getauxv(struct thread *td, struct proc *p, struct sbuf *sb)
{
	size_t vsize, size;
	char **auxv;
	int error;

	error = get_proc_vector(td, p, &auxv, &vsize, PROC_AUX);
	if (error == 0) {
#ifdef COMPAT_FREEBSD32
		if (SV_PROC_FLAG(p, SV_ILP32) != 0)
			size = vsize * sizeof(Elf32_Auxinfo);
		else
#endif
			size = vsize * sizeof(Elf_Auxinfo);
		if (sbuf_bcat(sb, auxv, size) != 0)
			error = ENOMEM;
		free(auxv, M_TEMP);
	}
	return (error);
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
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct pargs *newpa, *pa;
	struct proc *p;
	struct sbuf sb;
	int flags, error = 0, error2;
	pid_t pid;

	if (namelen != 1)
		return (EINVAL);

	p = curproc;
	pid = (pid_t)name[0];
	if (pid == -1) {
		pid = p->p_pid;
	}

	/*
	 * If the query is for this process and it is single-threaded, there
	 * is nobody to modify pargs, thus we can just read.
	 */
	if (pid == p->p_pid && p->p_numthreads == 1 && req->newptr == NULL &&
	    (pa = p->p_args) != NULL)
		return (SYSCTL_OUT(req, pa->ar_args, pa->ar_length));

	flags = PGET_CANSEE;
	if (req->newptr != NULL)
		flags |= PGET_ISCURRENT;
	error = pget(pid, flags, &p);
	if (error)
		return (error);

	pa = p->p_args;
	if (pa != NULL) {
		pargs_hold(pa);
		PROC_UNLOCK(p);
		error = SYSCTL_OUT(req, pa->ar_args, pa->ar_length);
		pargs_drop(pa);
	} else if ((p->p_flag & (P_WEXIT | P_SYSTEM)) == 0) {
		_PHOLD(p);
		PROC_UNLOCK(p);
		sbuf_new_for_sysctl(&sb, NULL, GET_PS_STRINGS_CHUNK_SZ, req);
		sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
		error = proc_getargv(curthread, p, &sb);
		error2 = sbuf_finish(&sb);
		PRELE(p);
		sbuf_delete(&sb);
		if (error == 0 && error2 != 0)
			error = error2;
	} else {
		PROC_UNLOCK(p);
	}
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (req->newlen > ps_arg_cache_limit - sizeof(struct pargs))
		return (ENOMEM);

	if (req->newlen == 0) {
		/*
		 * Clear the argument pointer, so that we'll fetch arguments
		 * with proc_getargv() until further notice.
		 */
		newpa = NULL;
	} else {
		newpa = pargs_alloc(req->newlen);
		error = SYSCTL_IN(req, newpa->ar_args, req->newlen);
		if (error != 0) {
			pargs_free(newpa);
			return (error);
		}
	}
	PROC_LOCK(p);
	pa = p->p_args;
	p->p_args = newpa;
	PROC_UNLOCK(p);
	pargs_drop(pa);
	return (0);
}

/*
 * This sysctl allows a process to retrieve environment of another process.
 */
static int
sysctl_kern_proc_env(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct sbuf sb;
	int error, error2;

	if (namelen != 1)
		return (EINVAL);

	error = pget((pid_t)name[0], PGET_WANTREAD, &p);
	if (error != 0)
		return (error);
	if ((p->p_flag & P_SYSTEM) != 0) {
		PRELE(p);
		return (0);
	}

	sbuf_new_for_sysctl(&sb, NULL, GET_PS_STRINGS_CHUNK_SZ, req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = proc_getenvv(curthread, p, &sb);
	error2 = sbuf_finish(&sb);
	PRELE(p);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

/*
 * This sysctl allows a process to retrieve ELF auxiliary vector of
 * another process.
 */
static int
sysctl_kern_proc_auxv(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct sbuf sb;
	int error, error2;

	if (namelen != 1)
		return (EINVAL);

	error = pget((pid_t)name[0], PGET_WANTREAD, &p);
	if (error != 0)
		return (error);
	if ((p->p_flag & P_SYSTEM) != 0) {
		PRELE(p);
		return (0);
	}
	sbuf_new_for_sysctl(&sb, NULL, GET_PS_STRINGS_CHUNK_SZ, req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = proc_getauxv(curthread, p, &sb);
	error2 = sbuf_finish(&sb);
	PRELE(p);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

/*
 * Look up the canonical executable path running in the specified process.
 * It tries to return the same hardlink name as was used for execve(2).
 * This allows the programs that modify their behavior based on their progname,
 * to operate correctly.
 *
 * Result is returned in retbuf, it must not be freed, similar to vn_fullpath()
 *   calling conventions.
 * binname is a pointer to temporary string buffer of length MAXPATHLEN,
 *   allocated and freed by caller.
 * freebuf should be freed by caller, from the M_TEMP malloc type.
 */
int
proc_get_binpath(struct proc *p, char *binname, char **retbuf,
    char **freebuf)
{
	struct nameidata nd;
	struct vnode *vp, *dvp;
	size_t freepath_size;
	int error;
	bool do_fullpath;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	vp = p->p_textvp;
	if (vp == NULL) {
		PROC_UNLOCK(p);
		*retbuf = "";
		*freebuf = NULL;
		return (0);
	}
	vref(vp);
	dvp = p->p_textdvp;
	if (dvp != NULL)
		vref(dvp);
	if (p->p_binname != NULL)
		strlcpy(binname, p->p_binname, MAXPATHLEN);
	PROC_UNLOCK(p);

	do_fullpath = true;
	*freebuf = NULL;
	if (dvp != NULL && binname[0] != '\0') {
		freepath_size = MAXPATHLEN;
		if (vn_fullpath_hardlink(vp, dvp, binname, strlen(binname),
		    retbuf, freebuf, &freepath_size) == 0) {
			/*
			 * Recheck the looked up path.  The binary
			 * might have been renamed or replaced, in
			 * which case we should not report old name.
			 */
			NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, *retbuf);
			error = namei(&nd);
			if (error == 0) {
				if (nd.ni_vp == vp)
					do_fullpath = false;
				vrele(nd.ni_vp);
				NDFREE_PNBUF(&nd);
			}
		}
	}
	if (do_fullpath) {
		free(*freebuf, M_TEMP);
		*freebuf = NULL;
		error = vn_fullpath(vp, retbuf, freebuf);
	}
	vrele(vp);
	if (dvp != NULL)
		vrele(dvp);
	return (error);
}

/*
 * This sysctl allows a process to retrieve the path of the executable for
 * itself or another process.
 */
static int
sysctl_kern_proc_pathname(SYSCTL_HANDLER_ARGS)
{
	pid_t *pidp = (pid_t *)arg1;
	unsigned int arglen = arg2;
	struct proc *p;
	char *retbuf, *freebuf, *binname;
	int error;

	if (arglen != 1)
		return (EINVAL);
	binname = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	binname[0] = '\0';
	if (*pidp == -1) {	/* -1 means this process */
		error = 0;
		p = req->td->td_proc;
		PROC_LOCK(p);
	} else {
		error = pget(*pidp, PGET_CANSEE, &p);
	}

	if (error == 0)
		error = proc_get_binpath(p, binname, &retbuf, &freebuf);
	free(binname, M_TEMP);
	if (error != 0)
		return (error);
	error = SYSCTL_OUT(req, retbuf, strlen(retbuf) + 1);
	free(freebuf, M_TEMP);
	return (error);
}

static int
sysctl_kern_proc_sv_name(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	char *sv_name;
	int *name;
	int namelen;
	int error;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;
	error = pget((pid_t)name[0], PGET_CANSEE, &p);
	if (error != 0)
		return (error);
	sv_name = p->p_sysent->sv_name;
	PROC_UNLOCK(p);
	return (sysctl_handle_string(oidp, sv_name, 0, req));
}

#ifdef KINFO_OVMENTRY_SIZE
CTASSERT(sizeof(struct kinfo_ovmentry) == KINFO_OVMENTRY_SIZE);
#endif

#ifdef COMPAT_FREEBSD7
static int
sysctl_kern_proc_ovmmap(SYSCTL_HANDLER_ARGS)
{
	vm_map_entry_t entry, tmp_entry;
	unsigned int last_timestamp, namelen;
	char *fullpath, *freepath;
	struct kinfo_ovmentry *kve;
	struct vattr va;
	struct ucred *cred;
	int error, *name;
	struct vnode *vp;
	struct proc *p;
	vm_map_t map;
	struct vmspace *vm;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;
	error = pget((pid_t)name[0], PGET_WANTREAD, &p);
	if (error != 0)
		return (error);
	vm = vmspace_acquire_ref(p);
	if (vm == NULL) {
		PRELE(p);
		return (ESRCH);
	}
	kve = malloc(sizeof(*kve), M_TEMP, M_WAITOK);

	map = &vm->vm_map;
	vm_map_lock_read(map);
	VM_MAP_ENTRY_FOREACH(entry, map) {
		vm_object_t obj, tobj, lobj;
		vm_offset_t addr;

		if (entry->eflags & MAP_ENTRY_IS_SUB_MAP)
			continue;

		bzero(kve, sizeof(*kve));
		kve->kve_structsize = sizeof(*kve);

		kve->kve_private_resident = 0;
		obj = entry->object.vm_object;
		if (obj != NULL) {
			VM_OBJECT_RLOCK(obj);
			if (obj->shadow_count == 1)
				kve->kve_private_resident =
				    obj->resident_page_count;
		}
		kve->kve_resident = 0;
		addr = entry->start;
		while (addr < entry->end) {
			if (pmap_extract(map->pmap, addr))
				kve->kve_resident++;
			addr += PAGE_SIZE;
		}

		for (lobj = tobj = obj; tobj; tobj = tobj->backing_object) {
			if (tobj != obj) {
				VM_OBJECT_RLOCK(tobj);
				kve->kve_offset += tobj->backing_object_offset;
			}
			if (lobj != obj)
				VM_OBJECT_RUNLOCK(lobj);
			lobj = tobj;
		}

		kve->kve_start = (void*)entry->start;
		kve->kve_end = (void*)entry->end;
		kve->kve_offset += (off_t)entry->offset;

		if (entry->protection & VM_PROT_READ)
			kve->kve_protection |= KVME_PROT_READ;
		if (entry->protection & VM_PROT_WRITE)
			kve->kve_protection |= KVME_PROT_WRITE;
		if (entry->protection & VM_PROT_EXECUTE)
			kve->kve_protection |= KVME_PROT_EXEC;

		if (entry->eflags & MAP_ENTRY_COW)
			kve->kve_flags |= KVME_FLAG_COW;
		if (entry->eflags & MAP_ENTRY_NEEDS_COPY)
			kve->kve_flags |= KVME_FLAG_NEEDS_COPY;
		if (entry->eflags & MAP_ENTRY_NOCOREDUMP)
			kve->kve_flags |= KVME_FLAG_NOCOREDUMP;

		last_timestamp = map->timestamp;
		vm_map_unlock_read(map);

		kve->kve_fileid = 0;
		kve->kve_fsid = 0;
		freepath = NULL;
		fullpath = "";
		if (lobj) {
			kve->kve_type = vm_object_kvme_type(lobj, &vp);
			if (kve->kve_type == KVME_TYPE_MGTDEVICE)
				kve->kve_type = KVME_TYPE_UNKNOWN;
			if (vp != NULL)
				vref(vp);
			if (lobj != obj)
				VM_OBJECT_RUNLOCK(lobj);

			kve->kve_ref_count = obj->ref_count;
			kve->kve_shadow_count = obj->shadow_count;
			VM_OBJECT_RUNLOCK(obj);
			if (vp != NULL) {
				vn_fullpath(vp, &fullpath, &freepath);
				cred = curthread->td_ucred;
				vn_lock(vp, LK_SHARED | LK_RETRY);
				if (VOP_GETATTR(vp, &va, cred) == 0) {
					kve->kve_fileid = va.va_fileid;
					/* truncate */
					kve->kve_fsid = va.va_fsid;
				}
				vput(vp);
			}
		} else {
			kve->kve_type = KVME_TYPE_NONE;
			kve->kve_ref_count = 0;
			kve->kve_shadow_count = 0;
		}

		strlcpy(kve->kve_path, fullpath, sizeof(kve->kve_path));
		if (freepath != NULL)
			free(freepath, M_TEMP);

		error = SYSCTL_OUT(req, kve, sizeof(*kve));
		vm_map_lock_read(map);
		if (error)
			break;
		if (last_timestamp != map->timestamp) {
			vm_map_lookup_entry(map, addr - 1, &tmp_entry);
			entry = tmp_entry;
		}
	}
	vm_map_unlock_read(map);
	vmspace_free(vm);
	PRELE(p);
	free(kve, M_TEMP);
	return (error);
}
#endif	/* COMPAT_FREEBSD7 */

#ifdef KINFO_VMENTRY_SIZE
CTASSERT(sizeof(struct kinfo_vmentry) == KINFO_VMENTRY_SIZE);
#endif

void
kern_proc_vmmap_resident(vm_map_t map, vm_map_entry_t entry,
    int *resident_count, bool *super)
{
	vm_object_t obj, tobj;
	vm_page_t m, m_adv;
	vm_offset_t addr;
	vm_paddr_t pa;
	vm_pindex_t pi, pi_adv, pindex;
	int incore;

	*super = false;
	*resident_count = 0;
	if (vmmap_skip_res_cnt)
		return;

	pa = 0;
	obj = entry->object.vm_object;
	addr = entry->start;
	m_adv = NULL;
	pi = OFF_TO_IDX(entry->offset);
	for (; addr < entry->end; addr += IDX_TO_OFF(pi_adv), pi += pi_adv) {
		if (m_adv != NULL) {
			m = m_adv;
		} else {
			pi_adv = atop(entry->end - addr);
			pindex = pi;
			for (tobj = obj;; tobj = tobj->backing_object) {
				m = vm_radix_lookup_ge(&tobj->rtree, pindex);
				if (m != NULL) {
					if (m->pindex == pindex)
						break;
					if (pi_adv > m->pindex - pindex) {
						pi_adv = m->pindex - pindex;
						m_adv = m;
					}
				}
				if (tobj->backing_object == NULL)
					goto next;
				pindex += OFF_TO_IDX(tobj->
				    backing_object_offset);
			}
		}
		m_adv = NULL;
		if (m->psind != 0 && addr + pagesizes[1] <= entry->end &&
		    (addr & (pagesizes[1] - 1)) == 0 && (incore =
		    pmap_mincore(map->pmap, addr, &pa) & MINCORE_SUPER) != 0) {
			*super = true;
			/*
			 * The virtual page might be smaller than the physical
			 * page, so we use the page size reported by the pmap
			 * rather than m->psind.
			 */
			pi_adv = atop(pagesizes[incore >> MINCORE_PSIND_SHIFT]);
		} else {
			/*
			 * We do not test the found page on validity.
			 * Either the page is busy and being paged in,
			 * or it was invalidated.  The first case
			 * should be counted as resident, the second
			 * is not so clear; we do account both.
			 */
			pi_adv = 1;
		}
		*resident_count += pi_adv;
next:;
	}
}

/*
 * Must be called with the process locked and will return unlocked.
 */
int
kern_proc_vmmap_out(struct proc *p, struct sbuf *sb, ssize_t maxlen, int flags)
{
	vm_map_entry_t entry, tmp_entry;
	struct vattr va;
	vm_map_t map;
	vm_object_t lobj, nobj, obj, tobj;
	char *fullpath, *freepath;
	struct kinfo_vmentry *kve;
	struct ucred *cred;
	struct vnode *vp;
	struct vmspace *vm;
	vm_offset_t addr;
	unsigned int last_timestamp;
	int error;
	key_t key;
	unsigned short seq;
	bool guard, super;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	_PHOLD(p);
	PROC_UNLOCK(p);
	vm = vmspace_acquire_ref(p);
	if (vm == NULL) {
		PRELE(p);
		return (ESRCH);
	}
	kve = malloc(sizeof(*kve), M_TEMP, M_WAITOK | M_ZERO);

	error = 0;
	map = &vm->vm_map;
	vm_map_lock_read(map);
	VM_MAP_ENTRY_FOREACH(entry, map) {
		if (entry->eflags & MAP_ENTRY_IS_SUB_MAP)
			continue;

		addr = entry->end;
		bzero(kve, sizeof(*kve));
		obj = entry->object.vm_object;
		if (obj != NULL) {
			if ((obj->flags & OBJ_ANON) != 0)
				kve->kve_obj = (uintptr_t)obj;

			for (tobj = obj; tobj != NULL;
			    tobj = tobj->backing_object) {
				VM_OBJECT_RLOCK(tobj);
				kve->kve_offset += tobj->backing_object_offset;
				lobj = tobj;
			}
			if (obj->backing_object == NULL)
				kve->kve_private_resident =
				    obj->resident_page_count;
			kern_proc_vmmap_resident(map, entry,
			    &kve->kve_resident, &super);
			if (super)
				kve->kve_flags |= KVME_FLAG_SUPER;
			for (tobj = obj; tobj != NULL; tobj = nobj) {
				nobj = tobj->backing_object;
				if (tobj != obj && tobj != lobj)
					VM_OBJECT_RUNLOCK(tobj);
			}
		} else {
			lobj = NULL;
		}

		kve->kve_start = entry->start;
		kve->kve_end = entry->end;
		kve->kve_offset += entry->offset;

		if (entry->protection & VM_PROT_READ)
			kve->kve_protection |= KVME_PROT_READ;
		if (entry->protection & VM_PROT_WRITE)
			kve->kve_protection |= KVME_PROT_WRITE;
		if (entry->protection & VM_PROT_EXECUTE)
			kve->kve_protection |= KVME_PROT_EXEC;
		if (entry->max_protection & VM_PROT_READ)
			kve->kve_protection |= KVME_MAX_PROT_READ;
		if (entry->max_protection & VM_PROT_WRITE)
			kve->kve_protection |= KVME_MAX_PROT_WRITE;
		if (entry->max_protection & VM_PROT_EXECUTE)
			kve->kve_protection |= KVME_MAX_PROT_EXEC;

		if (entry->eflags & MAP_ENTRY_COW)
			kve->kve_flags |= KVME_FLAG_COW;
		if (entry->eflags & MAP_ENTRY_NEEDS_COPY)
			kve->kve_flags |= KVME_FLAG_NEEDS_COPY;
		if (entry->eflags & MAP_ENTRY_NOCOREDUMP)
			kve->kve_flags |= KVME_FLAG_NOCOREDUMP;
		if (entry->eflags & MAP_ENTRY_GROWS_DOWN)
			kve->kve_flags |= KVME_FLAG_GROWS_DOWN;
		if (entry->eflags & MAP_ENTRY_USER_WIRED)
			kve->kve_flags |= KVME_FLAG_USER_WIRED;

		guard = (entry->eflags & MAP_ENTRY_GUARD) != 0;

		last_timestamp = map->timestamp;
		vm_map_unlock_read(map);

		freepath = NULL;
		fullpath = "";
		if (lobj != NULL) {
			kve->kve_type = vm_object_kvme_type(lobj, &vp);
			if (vp != NULL)
				vref(vp);
			if (lobj != obj)
				VM_OBJECT_RUNLOCK(lobj);

			kve->kve_ref_count = obj->ref_count;
			kve->kve_shadow_count = obj->shadow_count;
			if (obj->type == OBJT_DEVICE ||
			    obj->type == OBJT_MGTDEVICE) {
				cdev_pager_get_path(obj, kve->kve_path,
				    sizeof(kve->kve_path));
			}
			VM_OBJECT_RUNLOCK(obj);
			if ((lobj->flags & OBJ_SYSVSHM) != 0) {
				kve->kve_flags |= KVME_FLAG_SYSVSHM;
				shmobjinfo(lobj, &key, &seq);
				kve->kve_vn_fileid = key;
				kve->kve_vn_fsid_freebsd11 = seq;
			}
			if ((lobj->flags & OBJ_POSIXSHM) != 0) {
				kve->kve_flags |= KVME_FLAG_POSIXSHM;
				shm_get_path(lobj, kve->kve_path,
				    sizeof(kve->kve_path));
			}
			if (vp != NULL) {
				vn_fullpath(vp, &fullpath, &freepath);
				kve->kve_vn_type = vntype_to_kinfo(vp->v_type);
				cred = curthread->td_ucred;
				vn_lock(vp, LK_SHARED | LK_RETRY);
				if (VOP_GETATTR(vp, &va, cred) == 0) {
					kve->kve_vn_fileid = va.va_fileid;
					kve->kve_vn_fsid = va.va_fsid;
					kve->kve_vn_fsid_freebsd11 =
					    kve->kve_vn_fsid; /* truncate */
					kve->kve_vn_mode =
					    MAKEIMODE(va.va_type, va.va_mode);
					kve->kve_vn_size = va.va_size;
					kve->kve_vn_rdev = va.va_rdev;
					kve->kve_vn_rdev_freebsd11 =
					    kve->kve_vn_rdev; /* truncate */
					kve->kve_status = KF_ATTR_VALID;
				}
				vput(vp);
				strlcpy(kve->kve_path, fullpath, sizeof(
				    kve->kve_path));
				free(freepath, M_TEMP);
			}
		} else {
			kve->kve_type = guard ? KVME_TYPE_GUARD :
			    KVME_TYPE_NONE;
			kve->kve_ref_count = 0;
			kve->kve_shadow_count = 0;
		}

		/* Pack record size down */
		if ((flags & KERN_VMMAP_PACK_KINFO) != 0)
			kve->kve_structsize =
			    offsetof(struct kinfo_vmentry, kve_path) +
			    strlen(kve->kve_path) + 1;
		else
			kve->kve_structsize = sizeof(*kve);
		kve->kve_structsize = roundup(kve->kve_structsize,
		    sizeof(uint64_t));

		/* Halt filling and truncate rather than exceeding maxlen */
		if (maxlen != -1 && maxlen < kve->kve_structsize) {
			error = 0;
			vm_map_lock_read(map);
			break;
		} else if (maxlen != -1)
			maxlen -= kve->kve_structsize;

		if (sbuf_bcat(sb, kve, kve->kve_structsize) != 0)
			error = ENOMEM;
		vm_map_lock_read(map);
		if (error != 0)
			break;
		if (last_timestamp != map->timestamp) {
			vm_map_lookup_entry(map, addr - 1, &tmp_entry);
			entry = tmp_entry;
		}
	}
	vm_map_unlock_read(map);
	vmspace_free(vm);
	PRELE(p);
	free(kve, M_TEMP);
	return (error);
}

static int
sysctl_kern_proc_vmmap(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	struct sbuf sb;
	u_int namelen;
	int error, error2, *name;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;
	sbuf_new_for_sysctl(&sb, NULL, sizeof(struct kinfo_vmentry), req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = pget((pid_t)name[0], PGET_CANDEBUG | PGET_NOTWEXIT, &p);
	if (error != 0) {
		sbuf_delete(&sb);
		return (error);
	}
	error = kern_proc_vmmap_out(p, &sb, -1, KERN_VMMAP_PACK_KINFO);
	error2 = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

#if defined(STACK) || defined(DDB)
static int
sysctl_kern_proc_kstack(SYSCTL_HANDLER_ARGS)
{
	struct kinfo_kstack *kkstp;
	int error, i, *name, numthreads;
	lwpid_t *lwpidarray;
	struct thread *td;
	struct stack *st;
	struct sbuf sb;
	struct proc *p;
	u_int namelen;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;
	error = pget((pid_t)name[0], PGET_NOTINEXEC | PGET_WANTREAD, &p);
	if (error != 0)
		return (error);

	kkstp = malloc(sizeof(*kkstp), M_TEMP, M_WAITOK);
	st = stack_create(M_WAITOK);

	lwpidarray = NULL;
	PROC_LOCK(p);
	do {
		if (lwpidarray != NULL) {
			free(lwpidarray, M_TEMP);
			lwpidarray = NULL;
		}
		numthreads = p->p_numthreads;
		PROC_UNLOCK(p);
		lwpidarray = malloc(sizeof(*lwpidarray) * numthreads, M_TEMP,
		    M_WAITOK | M_ZERO);
		PROC_LOCK(p);
	} while (numthreads < p->p_numthreads);

	/*
	 * XXXRW: During the below loop, execve(2) and countless other sorts
	 * of changes could have taken place.  Should we check to see if the
	 * vmspace has been replaced, or the like, in order to prevent
	 * giving a snapshot that spans, say, execve(2), with some threads
	 * before and some after?  Among other things, the credentials could
	 * have changed, in which case the right to extract debug info might
	 * no longer be assured.
	 */
	i = 0;
	FOREACH_THREAD_IN_PROC(p, td) {
		KASSERT(i < numthreads,
		    ("sysctl_kern_proc_kstack: numthreads"));
		lwpidarray[i] = td->td_tid;
		i++;
	}
	PROC_UNLOCK(p);
	numthreads = i;
	for (i = 0; i < numthreads; i++) {
		td = tdfind(lwpidarray[i], p->p_pid);
		if (td == NULL) {
			continue;
		}
		bzero(kkstp, sizeof(*kkstp));
		(void)sbuf_new(&sb, kkstp->kkst_trace,
		    sizeof(kkstp->kkst_trace), SBUF_FIXEDLEN);
		thread_lock(td);
		kkstp->kkst_tid = td->td_tid;
		if (stack_save_td(st, td) == 0)
			kkstp->kkst_state = KKST_STATE_STACKOK;
		else
			kkstp->kkst_state = KKST_STATE_RUNNING;
		thread_unlock(td);
		PROC_UNLOCK(p);
		stack_sbuf_print(&sb, st);
		sbuf_finish(&sb);
		sbuf_delete(&sb);
		error = SYSCTL_OUT(req, kkstp, sizeof(*kkstp));
		if (error)
			break;
	}
	PRELE(p);
	if (lwpidarray != NULL)
		free(lwpidarray, M_TEMP);
	stack_destroy(st);
	free(kkstp, M_TEMP);
	return (error);
}
#endif

/*
 * This sysctl allows a process to retrieve the full list of groups from
 * itself or another process.
 */
static int
sysctl_kern_proc_groups(SYSCTL_HANDLER_ARGS)
{
	pid_t *pidp = (pid_t *)arg1;
	unsigned int arglen = arg2;
	struct proc *p;
	struct ucred *cred;
	int error;

	if (arglen != 1)
		return (EINVAL);
	if (*pidp == -1) {	/* -1 means this process */
		p = req->td->td_proc;
		PROC_LOCK(p);
	} else {
		error = pget(*pidp, PGET_CANSEE, &p);
		if (error != 0)
			return (error);
	}

	cred = crhold(p->p_ucred);
	PROC_UNLOCK(p);

	error = SYSCTL_OUT(req, cred->cr_groups,
	    cred->cr_ngroups * sizeof(gid_t));
	crfree(cred);
	return (error);
}

/*
 * This sysctl allows a process to retrieve or/and set the resource limit for
 * another process.
 */
static int
sysctl_kern_proc_rlimit(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct rlimit rlim;
	struct proc *p;
	u_int which;
	int flags, error;

	if (namelen != 2)
		return (EINVAL);

	which = (u_int)name[1];
	if (which >= RLIM_NLIMITS)
		return (EINVAL);

	if (req->newptr != NULL && req->newlen != sizeof(rlim))
		return (EINVAL);

	flags = PGET_HOLD | PGET_NOTWEXIT;
	if (req->newptr != NULL)
		flags |= PGET_CANDEBUG;
	else
		flags |= PGET_CANSEE;
	error = pget((pid_t)name[0], flags, &p);
	if (error != 0)
		return (error);

	/*
	 * Retrieve limit.
	 */
	if (req->oldptr != NULL) {
		PROC_LOCK(p);
		lim_rlimit_proc(p, which, &rlim);
		PROC_UNLOCK(p);
	}
	error = SYSCTL_OUT(req, &rlim, sizeof(rlim));
	if (error != 0)
		goto errout;

	/*
	 * Set limit.
	 */
	if (req->newptr != NULL) {
		error = SYSCTL_IN(req, &rlim, sizeof(rlim));
		if (error == 0)
			error = kern_proc_setrlimit(curthread, p, which, &rlim);
	}

errout:
	PRELE(p);
	return (error);
}

/*
 * This sysctl allows a process to retrieve ps_strings structure location of
 * another process.
 */
static int
sysctl_kern_proc_ps_strings(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	vm_offset_t ps_strings;
	int error;
#ifdef COMPAT_FREEBSD32
	uint32_t ps_strings32;
#endif

	if (namelen != 1)
		return (EINVAL);

	error = pget((pid_t)name[0], PGET_CANDEBUG, &p);
	if (error != 0)
		return (error);
#ifdef COMPAT_FREEBSD32
	if ((req->flags & SCTL_MASK32) != 0) {
		/*
		 * We return 0 if the 32 bit emulation request is for a 64 bit
		 * process.
		 */
		ps_strings32 = SV_PROC_FLAG(p, SV_ILP32) != 0 ?
		    PTROUT(PROC_PS_STRINGS(p)) : 0;
		PROC_UNLOCK(p);
		error = SYSCTL_OUT(req, &ps_strings32, sizeof(ps_strings32));
		return (error);
	}
#endif
	ps_strings = PROC_PS_STRINGS(p);
	PROC_UNLOCK(p);
	error = SYSCTL_OUT(req, &ps_strings, sizeof(ps_strings));
	return (error);
}

/*
 * This sysctl allows a process to retrieve umask of another process.
 */
static int
sysctl_kern_proc_umask(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	int error;
	u_short cmask;
	pid_t pid;

	if (namelen != 1)
		return (EINVAL);

	pid = (pid_t)name[0];
	p = curproc;
	if (pid == p->p_pid || pid == 0) {
		cmask = p->p_pd->pd_cmask;
		goto out;
	}

	error = pget(pid, PGET_WANTREAD, &p);
	if (error != 0)
		return (error);

	cmask = p->p_pd->pd_cmask;
	PRELE(p);
out:
	error = SYSCTL_OUT(req, &cmask, sizeof(cmask));
	return (error);
}

/*
 * This sysctl allows a process to set and retrieve binary osreldate of
 * another process.
 */
static int
sysctl_kern_proc_osrel(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	int flags, error, osrel;

	if (namelen != 1)
		return (EINVAL);

	if (req->newptr != NULL && req->newlen != sizeof(osrel))
		return (EINVAL);

	flags = PGET_HOLD | PGET_NOTWEXIT;
	if (req->newptr != NULL)
		flags |= PGET_CANDEBUG;
	else
		flags |= PGET_CANSEE;
	error = pget((pid_t)name[0], flags, &p);
	if (error != 0)
		return (error);

	error = SYSCTL_OUT(req, &p->p_osrel, sizeof(p->p_osrel));
	if (error != 0)
		goto errout;

	if (req->newptr != NULL) {
		error = SYSCTL_IN(req, &osrel, sizeof(osrel));
		if (error != 0)
			goto errout;
		if (osrel < 0) {
			error = EINVAL;
			goto errout;
		}
		p->p_osrel = osrel;
	}
errout:
	PRELE(p);
	return (error);
}

static int
sysctl_kern_proc_sigtramp(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct kinfo_sigtramp kst;
	const struct sysentvec *sv;
	int error;
#ifdef COMPAT_FREEBSD32
	struct kinfo_sigtramp32 kst32;
#endif

	if (namelen != 1)
		return (EINVAL);

	error = pget((pid_t)name[0], PGET_CANDEBUG, &p);
	if (error != 0)
		return (error);
	sv = p->p_sysent;
#ifdef COMPAT_FREEBSD32
	if ((req->flags & SCTL_MASK32) != 0) {
		bzero(&kst32, sizeof(kst32));
		if (SV_PROC_FLAG(p, SV_ILP32)) {
			if (PROC_HAS_SHP(p)) {
				kst32.ksigtramp_start = PROC_SIGCODE(p);
				kst32.ksigtramp_end = kst32.ksigtramp_start +
				    ((sv->sv_flags & SV_DSO_SIG) == 0 ?
				    *sv->sv_szsigcode :
				    (uintptr_t)sv->sv_szsigcode);
			} else {
				kst32.ksigtramp_start = PROC_PS_STRINGS(p) -
				    *sv->sv_szsigcode;
				kst32.ksigtramp_end = PROC_PS_STRINGS(p);
			}
		}
		PROC_UNLOCK(p);
		error = SYSCTL_OUT(req, &kst32, sizeof(kst32));
		return (error);
	}
#endif
	bzero(&kst, sizeof(kst));
	if (PROC_HAS_SHP(p)) {
		kst.ksigtramp_start = (char *)PROC_SIGCODE(p);
		kst.ksigtramp_end = (char *)kst.ksigtramp_start +
		    ((sv->sv_flags & SV_DSO_SIG) == 0 ? *sv->sv_szsigcode :
		    (uintptr_t)sv->sv_szsigcode);
	} else {
		kst.ksigtramp_start = (char *)PROC_PS_STRINGS(p) -
		    *sv->sv_szsigcode;
		kst.ksigtramp_end = (char *)PROC_PS_STRINGS(p);
	}
	PROC_UNLOCK(p);
	error = SYSCTL_OUT(req, &kst, sizeof(kst));
	return (error);
}

static int
sysctl_kern_proc_sigfastblk(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	pid_t pid;
	struct proc *p;
	struct thread *td1;
	uintptr_t addr;
#ifdef COMPAT_FREEBSD32
	uint32_t addr32;
#endif
	int error;

	if (namelen != 1 || req->newptr != NULL)
		return (EINVAL);

	pid = (pid_t)name[0];
	error = pget(pid, PGET_HOLD | PGET_NOTWEXIT | PGET_CANDEBUG, &p);
	if (error != 0)
		return (error);

	PROC_LOCK(p);
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		if (!SV_PROC_FLAG(p, SV_ILP32)) {
			error = EINVAL;
			goto errlocked;
		}
	}
#endif
	if (pid <= PID_MAX) {
		td1 = FIRST_THREAD_IN_PROC(p);
	} else {
		FOREACH_THREAD_IN_PROC(p, td1) {
			if (td1->td_tid == pid)
				break;
		}
	}
	if (td1 == NULL) {
		error = ESRCH;
		goto errlocked;
	}
	/*
	 * The access to the private thread flags.  It is fine as far
	 * as no out-of-thin-air values are read from td_pflags, and
	 * usermode read of the td_sigblock_ptr is racy inherently,
	 * since target process might have already changed it
	 * meantime.
	 */
	if ((td1->td_pflags & TDP_SIGFASTBLOCK) != 0)
		addr = (uintptr_t)td1->td_sigblock_ptr;
	else
		error = ENOTTY;

errlocked:
	_PRELE(p);
	PROC_UNLOCK(p);
	if (error != 0)
		return (error);

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		addr32 = addr;
		error = SYSCTL_OUT(req, &addr32, sizeof(addr32));
	} else
#endif
		error = SYSCTL_OUT(req, &addr, sizeof(addr));
	return (error);
}

static int
sysctl_kern_proc_vm_layout(SYSCTL_HANDLER_ARGS)
{
	struct kinfo_vm_layout kvm;
	struct proc *p;
	struct vmspace *vmspace;
	int error, *name;

	name = (int *)arg1;
	if ((u_int)arg2 != 1)
		return (EINVAL);

	error = pget((pid_t)name[0], PGET_CANDEBUG, &p);
	if (error != 0)
		return (error);
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		if (!SV_PROC_FLAG(p, SV_ILP32)) {
			PROC_UNLOCK(p);
			return (EINVAL);
		}
	}
#endif
	vmspace = vmspace_acquire_ref(p);
	PROC_UNLOCK(p);

	memset(&kvm, 0, sizeof(kvm));
	kvm.kvm_min_user_addr = vm_map_min(&vmspace->vm_map);
	kvm.kvm_max_user_addr = vm_map_max(&vmspace->vm_map);
	kvm.kvm_text_addr = (uintptr_t)vmspace->vm_taddr;
	kvm.kvm_text_size = vmspace->vm_tsize;
	kvm.kvm_data_addr = (uintptr_t)vmspace->vm_daddr;
	kvm.kvm_data_size = vmspace->vm_dsize;
	kvm.kvm_stack_addr = (uintptr_t)vmspace->vm_maxsaddr;
	kvm.kvm_stack_size = vmspace->vm_ssize;
	kvm.kvm_shp_addr = vmspace->vm_shp_base;
	kvm.kvm_shp_size = p->p_sysent->sv_shared_page_len;
	if ((vmspace->vm_map.flags & MAP_WIREFUTURE) != 0)
		kvm.kvm_map_flags |= KMAP_FLAG_WIREFUTURE;
	if ((vmspace->vm_map.flags & MAP_ASLR) != 0)
		kvm.kvm_map_flags |= KMAP_FLAG_ASLR;
	if ((vmspace->vm_map.flags & MAP_ASLR_IGNSTART) != 0)
		kvm.kvm_map_flags |= KMAP_FLAG_ASLR_IGNSTART;
	if ((vmspace->vm_map.flags & MAP_WXORX) != 0)
		kvm.kvm_map_flags |= KMAP_FLAG_WXORX;
	if ((vmspace->vm_map.flags & MAP_ASLR_STACK) != 0)
		kvm.kvm_map_flags |= KMAP_FLAG_ASLR_STACK;
	if (vmspace->vm_shp_base != p->p_sysent->sv_shared_page_base &&
	    PROC_HAS_SHP(p))
		kvm.kvm_map_flags |= KMAP_FLAG_ASLR_SHARED_PAGE;

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		struct kinfo_vm_layout32 kvm32;

		memset(&kvm32, 0, sizeof(kvm32));
		kvm32.kvm_min_user_addr = (uint32_t)kvm.kvm_min_user_addr;
		kvm32.kvm_max_user_addr = (uint32_t)kvm.kvm_max_user_addr;
		kvm32.kvm_text_addr = (uint32_t)kvm.kvm_text_addr;
		kvm32.kvm_text_size = (uint32_t)kvm.kvm_text_size;
		kvm32.kvm_data_addr = (uint32_t)kvm.kvm_data_addr;
		kvm32.kvm_data_size = (uint32_t)kvm.kvm_data_size;
		kvm32.kvm_stack_addr = (uint32_t)kvm.kvm_stack_addr;
		kvm32.kvm_stack_size = (uint32_t)kvm.kvm_stack_size;
		kvm32.kvm_shp_addr = (uint32_t)kvm.kvm_shp_addr;
		kvm32.kvm_shp_size = (uint32_t)kvm.kvm_shp_size;
		kvm32.kvm_map_flags = kvm.kvm_map_flags;
		error = SYSCTL_OUT(req, &kvm32, sizeof(kvm32));
		goto out;
	}
#endif

	error = SYSCTL_OUT(req, &kvm, sizeof(kvm));
#ifdef COMPAT_FREEBSD32
out:
#endif
	vmspace_free(vmspace);
	return (error);
}

SYSCTL_NODE(_kern, KERN_PROC, proc, CTLFLAG_RD | CTLFLAG_MPSAFE,  0,
    "Process table");

SYSCTL_PROC(_kern_proc, KERN_PROC_ALL, all, CTLFLAG_RD|CTLTYPE_STRUCT|
	CTLFLAG_MPSAFE, 0, 0, sysctl_kern_proc, "S,proc",
	"Return entire process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_GID, gid, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_PGRP, pgrp, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_RGID, rgid, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_SESSION, sid, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_TTY, tty, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_UID, uid, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_RUID, ruid, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_PID, pid, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, KERN_PROC_PROC, proc, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc, "Return process table, no threads");

static SYSCTL_NODE(_kern_proc, KERN_PROC_ARGS, args,
	CTLFLAG_RW | CTLFLAG_CAPWR | CTLFLAG_ANYBODY | CTLFLAG_MPSAFE,
	sysctl_kern_proc_args, "Process argument list");

static SYSCTL_NODE(_kern_proc, KERN_PROC_ENV, env, CTLFLAG_RD | CTLFLAG_MPSAFE,
	sysctl_kern_proc_env, "Process environment");

static SYSCTL_NODE(_kern_proc, KERN_PROC_AUXV, auxv, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_auxv, "Process ELF auxiliary vector");

static SYSCTL_NODE(_kern_proc, KERN_PROC_PATHNAME, pathname, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_pathname, "Process executable path");

static SYSCTL_NODE(_kern_proc, KERN_PROC_SV_NAME, sv_name, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_sv_name,
	"Process syscall vector name (ABI type)");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_GID | KERN_PROC_INC_THREAD), gid_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_PGRP | KERN_PROC_INC_THREAD), pgrp_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_RGID | KERN_PROC_INC_THREAD), rgid_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_SESSION | KERN_PROC_INC_THREAD),
	sid_td, CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_TTY | KERN_PROC_INC_THREAD), tty_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_UID | KERN_PROC_INC_THREAD), uid_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_RUID | KERN_PROC_INC_THREAD), ruid_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_PID | KERN_PROC_INC_THREAD), pid_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc, "Process table");

static SYSCTL_NODE(_kern_proc, (KERN_PROC_PROC | KERN_PROC_INC_THREAD), proc_td,
	CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_kern_proc,
	"Return process table, including threads");

#ifdef COMPAT_FREEBSD7
static SYSCTL_NODE(_kern_proc, KERN_PROC_OVMMAP, ovmmap, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_ovmmap, "Old Process vm map entries");
#endif

static SYSCTL_NODE(_kern_proc, KERN_PROC_VMMAP, vmmap, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_vmmap, "Process vm map entries");

#if defined(STACK) || defined(DDB)
static SYSCTL_NODE(_kern_proc, KERN_PROC_KSTACK, kstack, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_kstack, "Process kernel stacks");
#endif

static SYSCTL_NODE(_kern_proc, KERN_PROC_GROUPS, groups, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_groups, "Process groups");

static SYSCTL_NODE(_kern_proc, KERN_PROC_RLIMIT, rlimit, CTLFLAG_RW |
	CTLFLAG_ANYBODY | CTLFLAG_MPSAFE, sysctl_kern_proc_rlimit,
	"Process resource limits");

static SYSCTL_NODE(_kern_proc, KERN_PROC_PS_STRINGS, ps_strings, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_ps_strings,
	"Process ps_strings location");

static SYSCTL_NODE(_kern_proc, KERN_PROC_UMASK, umask, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_umask, "Process umask");

static SYSCTL_NODE(_kern_proc, KERN_PROC_OSREL, osrel, CTLFLAG_RW |
	CTLFLAG_ANYBODY | CTLFLAG_MPSAFE, sysctl_kern_proc_osrel,
	"Process binary osreldate");

static SYSCTL_NODE(_kern_proc, KERN_PROC_SIGTRAMP, sigtramp, CTLFLAG_RD |
	CTLFLAG_MPSAFE, sysctl_kern_proc_sigtramp,
	"Process signal trampoline location");

static SYSCTL_NODE(_kern_proc, KERN_PROC_SIGFASTBLK, sigfastblk, CTLFLAG_RD |
	CTLFLAG_ANYBODY | CTLFLAG_MPSAFE, sysctl_kern_proc_sigfastblk,
	"Thread sigfastblock address");

static SYSCTL_NODE(_kern_proc, KERN_PROC_VM_LAYOUT, vm_layout, CTLFLAG_RD |
	CTLFLAG_ANYBODY | CTLFLAG_MPSAFE, sysctl_kern_proc_vm_layout,
	"Process virtual address space layout info");

static struct sx stop_all_proc_blocker;
SX_SYSINIT(stop_all_proc_blocker, &stop_all_proc_blocker, "sapblk");

bool
stop_all_proc_block(void)
{
	return (sx_xlock_sig(&stop_all_proc_blocker) == 0);
}

void
stop_all_proc_unblock(void)
{
	sx_xunlock(&stop_all_proc_blocker);
}

int allproc_gen;

/*
 * stop_all_proc() purpose is to stop all process which have usermode,
 * except current process for obvious reasons.  This makes it somewhat
 * unreliable when invoked from multithreaded process.  The service
 * must not be user-callable anyway.
 */
void
stop_all_proc(void)
{
	struct proc *cp, *p;
	int r, gen;
	bool restart, seen_stopped, seen_exiting, stopped_some;

	if (!stop_all_proc_block())
		return;

	cp = curproc;
allproc_loop:
	sx_xlock(&allproc_lock);
	gen = allproc_gen;
	seen_exiting = seen_stopped = stopped_some = restart = false;
	LIST_REMOVE(cp, p_list);
	LIST_INSERT_HEAD(&allproc, cp, p_list);
	for (;;) {
		p = LIST_NEXT(cp, p_list);
		if (p == NULL)
			break;
		LIST_REMOVE(cp, p_list);
		LIST_INSERT_AFTER(p, cp, p_list);
		PROC_LOCK(p);
		if ((p->p_flag & (P_KPROC | P_SYSTEM | P_TOTAL_STOP |
		    P_STOPPED_SIG)) != 0) {
			PROC_UNLOCK(p);
			continue;
		}
		if ((p->p_flag2 & P2_WEXIT) != 0) {
			seen_exiting = true;
			PROC_UNLOCK(p);
			continue;
		}
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			/*
			 * Stopped processes are tolerated when there
			 * are no other processes which might continue
			 * them.  P_STOPPED_SINGLE but not
			 * P_TOTAL_STOP process still has at least one
			 * thread running.
			 */
			seen_stopped = true;
			PROC_UNLOCK(p);
			continue;
		}
		if ((p->p_flag & P_TRACED) != 0) {
			/*
			 * thread_single() below cannot stop traced p,
			 * so skip it.  OTOH, we cannot require
			 * restart because debugger might be either
			 * already stopped or traced as well.
			 */
			PROC_UNLOCK(p);
			continue;
		}
		sx_xunlock(&allproc_lock);
		_PHOLD(p);
		r = thread_single(p, SINGLE_ALLPROC);
		if (r != 0)
			restart = true;
		else
			stopped_some = true;
		_PRELE(p);
		PROC_UNLOCK(p);
		sx_xlock(&allproc_lock);
	}
	/* Catch forked children we did not see in iteration. */
	if (gen != allproc_gen)
		restart = true;
	sx_xunlock(&allproc_lock);
	if (restart || stopped_some || seen_exiting || seen_stopped) {
		kern_yield(PRI_USER);
		goto allproc_loop;
	}
}

void
resume_all_proc(void)
{
	struct proc *cp, *p;

	cp = curproc;
	sx_xlock(&allproc_lock);
again:
	LIST_REMOVE(cp, p_list);
	LIST_INSERT_HEAD(&allproc, cp, p_list);
	for (;;) {
		p = LIST_NEXT(cp, p_list);
		if (p == NULL)
			break;
		LIST_REMOVE(cp, p_list);
		LIST_INSERT_AFTER(p, cp, p_list);
		PROC_LOCK(p);
		if ((p->p_flag & P_TOTAL_STOP) != 0) {
			sx_xunlock(&allproc_lock);
			_PHOLD(p);
			thread_single_end(p, SINGLE_ALLPROC);
			_PRELE(p);
			PROC_UNLOCK(p);
			sx_xlock(&allproc_lock);
		} else {
			PROC_UNLOCK(p);
		}
	}
	/*  Did the loop above missed any stopped process ? */
	FOREACH_PROC_IN_SYSTEM(p) {
		/* No need for proc lock. */
		if ((p->p_flag & P_TOTAL_STOP) != 0)
			goto again;
	}
	sx_xunlock(&allproc_lock);

	stop_all_proc_unblock();
}

/* #define	TOTAL_STOP_DEBUG	1 */
#ifdef TOTAL_STOP_DEBUG
volatile static int ap_resume;
#include <sys/mount.h>

static int
sysctl_debug_stop_all_proc(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = 0;
	ap_resume = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0) {
		stop_all_proc();
		syncer_suspend();
		while (ap_resume == 0)
			;
		syncer_resume();
		resume_all_proc();
	}
	return (0);
}

SYSCTL_PROC(_debug, OID_AUTO, stop_all_proc, CTLTYPE_INT | CTLFLAG_RW |
    CTLFLAG_MPSAFE, __DEVOLATILE(int *, &ap_resume), 0,
    sysctl_debug_stop_all_proc, "I",
    "");
#endif
