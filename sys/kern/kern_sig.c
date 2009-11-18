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
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_kdtrace.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/condvar.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/posix4.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/sdt.h>
#include <sys/sbuf.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/timers.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <machine/cpu.h>

#include <security/audit/audit.h>

#define	ONSIG	32		/* NSIG for osig* syscalls.  XXX. */

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE(proc, kernel, , signal_send);
SDT_PROBE_ARGTYPE(proc, kernel, , signal_send, 0, "struct thread *");
SDT_PROBE_ARGTYPE(proc, kernel, , signal_send, 1, "struct proc *");
SDT_PROBE_ARGTYPE(proc, kernel, , signal_send, 2, "int");
SDT_PROBE_DEFINE(proc, kernel, , signal_clear);
SDT_PROBE_ARGTYPE(proc, kernel, , signal_clear, 0, "int");
SDT_PROBE_ARGTYPE(proc, kernel, , signal_clear, 1, "ksiginfo_t *");
SDT_PROBE_DEFINE(proc, kernel, , signal_discard);
SDT_PROBE_ARGTYPE(proc, kernel, , signal_discard, 0, "struct thread *");
SDT_PROBE_ARGTYPE(proc, kernel, , signal_discard, 1, "struct proc *");
SDT_PROBE_ARGTYPE(proc, kernel, , signal_discard, 2, "int");

static int	coredump(struct thread *);
static char	*expand_name(const char *, uid_t, pid_t);
static int	killpg1(struct thread *td, int sig, int pgid, int all,
		    ksiginfo_t *ksi);
static int	issignal(struct thread *td, int stop_allowed);
static int	sigprop(int sig);
static void	tdsigwakeup(struct thread *, int, sig_t, int);
static void	sig_suspend_threads(struct thread *, struct proc *, int);
static int	filt_sigattach(struct knote *kn);
static void	filt_sigdetach(struct knote *kn);
static int	filt_signal(struct knote *kn, long hint);
static struct thread *sigtd(struct proc *p, int sig, int prop);
static void	sigqueue_start(void);

static uma_zone_t	ksiginfo_zone = NULL;
struct filterops sig_filtops = {
	.f_isfd = 0,
	.f_attach = filt_sigattach,
	.f_detach = filt_sigdetach,
	.f_event = filt_signal,
};

int	kern_logsigexit = 1;
SYSCTL_INT(_kern, KERN_LOGSIGEXIT, logsigexit, CTLFLAG_RW, 
    &kern_logsigexit, 0, 
    "Log processes quitting on abnormal signals to syslog(3)");

static int	kern_forcesigexit = 1;
SYSCTL_INT(_kern, OID_AUTO, forcesigexit, CTLFLAG_RW,
    &kern_forcesigexit, 0, "Force trap signal to be handled");

SYSCTL_NODE(_kern, OID_AUTO, sigqueue, CTLFLAG_RW, 0, "POSIX real time signal");

static int	max_pending_per_proc = 128;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, max_pending_per_proc, CTLFLAG_RW,
    &max_pending_per_proc, 0, "Max pending signals per proc");

static int	preallocate_siginfo = 1024;
TUNABLE_INT("kern.sigqueue.preallocate", &preallocate_siginfo);
SYSCTL_INT(_kern_sigqueue, OID_AUTO, preallocate, CTLFLAG_RD,
    &preallocate_siginfo, 0, "Preallocated signal memory size");

static int	signal_overflow = 0;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, overflow, CTLFLAG_RD,
    &signal_overflow, 0, "Number of signals overflew");

static int	signal_alloc_fail = 0;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, alloc_fail, CTLFLAG_RD,
    &signal_alloc_fail, 0, "signals failed to be allocated");

SYSINIT(signal, SI_SUB_P1003_1B, SI_ORDER_FIRST+3, sigqueue_start, NULL);

/*
 * Policy -- Can ucred cr1 send SIGIO to process cr2?
 * Should use cr_cansignal() once cr_cansignal() allows SIGIO and SIGURG
 * in the right situations.
 */
#define CANSIGIO(cr1, cr2) \
	((cr1)->cr_uid == 0 || \
	    (cr1)->cr_ruid == (cr2)->cr_ruid || \
	    (cr1)->cr_uid == (cr2)->cr_ruid || \
	    (cr1)->cr_ruid == (cr2)->cr_uid || \
	    (cr1)->cr_uid == (cr2)->cr_uid)

int sugid_coredump;
SYSCTL_INT(_kern, OID_AUTO, sugid_coredump, CTLFLAG_RW, 
    &sugid_coredump, 0, "Allow setuid and setgid processes to dump core");

static int	do_coredump = 1;
SYSCTL_INT(_kern, OID_AUTO, coredump, CTLFLAG_RW,
	&do_coredump, 0, "Enable/Disable coredumps");

static int	set_core_nodump_flag = 0;
SYSCTL_INT(_kern, OID_AUTO, nodump_coredump, CTLFLAG_RW, &set_core_nodump_flag,
	0, "Enable setting the NODUMP flag on coredump files");

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SA_KILL		0x01		/* terminates process by default */
#define	SA_CORE		0x02		/* ditto and coredumps */
#define	SA_STOP		0x04		/* suspend process */
#define	SA_TTYSTOP	0x08		/* ditto, from tty */
#define	SA_IGNORE	0x10		/* ignore by default */
#define	SA_CONT		0x20		/* continue if suspended */
#define	SA_CANTMASK	0x40		/* non-maskable, catchable */
#define	SA_PROC		0x80		/* deliverable to any thread */

static int sigproptbl[NSIG] = {
        SA_KILL|SA_PROC,		/* SIGHUP */
        SA_KILL|SA_PROC,		/* SIGINT */
        SA_KILL|SA_CORE|SA_PROC,	/* SIGQUIT */
        SA_KILL|SA_CORE,		/* SIGILL */
        SA_KILL|SA_CORE,		/* SIGTRAP */
        SA_KILL|SA_CORE,		/* SIGABRT */
        SA_KILL|SA_CORE|SA_PROC,	/* SIGEMT */
        SA_KILL|SA_CORE,		/* SIGFPE */
        SA_KILL|SA_PROC,		/* SIGKILL */
        SA_KILL|SA_CORE,		/* SIGBUS */
        SA_KILL|SA_CORE,		/* SIGSEGV */
        SA_KILL|SA_CORE,		/* SIGSYS */
        SA_KILL|SA_PROC,		/* SIGPIPE */
        SA_KILL|SA_PROC,		/* SIGALRM */
        SA_KILL|SA_PROC,		/* SIGTERM */
        SA_IGNORE|SA_PROC,		/* SIGURG */
        SA_STOP|SA_PROC,		/* SIGSTOP */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTSTP */
        SA_IGNORE|SA_CONT|SA_PROC,	/* SIGCONT */
        SA_IGNORE|SA_PROC,		/* SIGCHLD */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTTIN */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTTOU */
        SA_IGNORE|SA_PROC,		/* SIGIO */
        SA_KILL,			/* SIGXCPU */
        SA_KILL,			/* SIGXFSZ */
        SA_KILL|SA_PROC,		/* SIGVTALRM */
        SA_KILL|SA_PROC,		/* SIGPROF */
        SA_IGNORE|SA_PROC,		/* SIGWINCH  */
        SA_IGNORE|SA_PROC,		/* SIGINFO */
        SA_KILL|SA_PROC,		/* SIGUSR1 */
        SA_KILL|SA_PROC,		/* SIGUSR2 */
};

static void reschedule_signals(struct proc *p, sigset_t block, int flags);

static void
sigqueue_start(void)
{
	ksiginfo_zone = uma_zcreate("ksiginfo", sizeof(ksiginfo_t),
		NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_prealloc(ksiginfo_zone, preallocate_siginfo);
	p31b_setcfg(CTL_P1003_1B_REALTIME_SIGNALS, _POSIX_REALTIME_SIGNALS);
	p31b_setcfg(CTL_P1003_1B_RTSIG_MAX, SIGRTMAX - SIGRTMIN + 1);
	p31b_setcfg(CTL_P1003_1B_SIGQUEUE_MAX, max_pending_per_proc);
}

ksiginfo_t *
ksiginfo_alloc(int wait)
{
	int flags;

	flags = M_ZERO;
	if (! wait)
		flags |= M_NOWAIT;
	if (ksiginfo_zone != NULL)
		return ((ksiginfo_t *)uma_zalloc(ksiginfo_zone, flags));
	return (NULL);
}

void
ksiginfo_free(ksiginfo_t *ksi)
{
	uma_zfree(ksiginfo_zone, ksi);
}

static __inline int
ksiginfo_tryfree(ksiginfo_t *ksi)
{
	if (!(ksi->ksi_flags & KSI_EXT)) {
		uma_zfree(ksiginfo_zone, ksi);
		return (1);
	}
	return (0);
}

void
sigqueue_init(sigqueue_t *list, struct proc *p)
{
	SIGEMPTYSET(list->sq_signals);
	SIGEMPTYSET(list->sq_kill);
	TAILQ_INIT(&list->sq_list);
	list->sq_proc = p;
	list->sq_flags = SQ_INIT;
}

/*
 * Get a signal's ksiginfo.
 * Return:
 * 	0	-	signal not found
 *	others	-	signal number
 */ 
int
sigqueue_get(sigqueue_t *sq, int signo, ksiginfo_t *si)
{
	struct proc *p = sq->sq_proc;
	struct ksiginfo *ksi, *next;
	int count = 0;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));

	if (!SIGISMEMBER(sq->sq_signals, signo))
		return (0);

	if (SIGISMEMBER(sq->sq_kill, signo)) {
		count++;
		SIGDELSET(sq->sq_kill, signo);
	}

	TAILQ_FOREACH_SAFE(ksi, &sq->sq_list, ksi_link, next) {
		if (ksi->ksi_signo == signo) {
			if (count == 0) {
				TAILQ_REMOVE(&sq->sq_list, ksi, ksi_link);
				ksi->ksi_sigq = NULL;
				ksiginfo_copy(ksi, si);
				if (ksiginfo_tryfree(ksi) && p != NULL)
					p->p_pendingcnt--;
			}
			if (++count > 1)
				break;
		}
	}

	if (count <= 1)
		SIGDELSET(sq->sq_signals, signo);
	si->ksi_signo = signo;
	return (signo);
}

void
sigqueue_take(ksiginfo_t *ksi)
{
	struct ksiginfo *kp;
	struct proc	*p;
	sigqueue_t	*sq;

	if (ksi == NULL || (sq = ksi->ksi_sigq) == NULL)
		return;

	p = sq->sq_proc;
	TAILQ_REMOVE(&sq->sq_list, ksi, ksi_link);
	ksi->ksi_sigq = NULL;
	if (!(ksi->ksi_flags & KSI_EXT) && p != NULL)
		p->p_pendingcnt--;

	for (kp = TAILQ_FIRST(&sq->sq_list); kp != NULL;
	     kp = TAILQ_NEXT(kp, ksi_link)) {
		if (kp->ksi_signo == ksi->ksi_signo)
			break;
	}
	if (kp == NULL && !SIGISMEMBER(sq->sq_kill, ksi->ksi_signo))
		SIGDELSET(sq->sq_signals, ksi->ksi_signo);
}

int
sigqueue_add(sigqueue_t *sq, int signo, ksiginfo_t *si)
{
	struct proc *p = sq->sq_proc;
	struct ksiginfo *ksi;
	int ret = 0;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));
	
	if (signo == SIGKILL || signo == SIGSTOP || si == NULL) {
		SIGADDSET(sq->sq_kill, signo);
		goto out_set_bit;
	}

	/* directly insert the ksi, don't copy it */
	if (si->ksi_flags & KSI_INS) {
		TAILQ_INSERT_TAIL(&sq->sq_list, si, ksi_link);
		si->ksi_sigq = sq;
		goto out_set_bit;
	}

	if (__predict_false(ksiginfo_zone == NULL)) {
		SIGADDSET(sq->sq_kill, signo);
		goto out_set_bit;
	}
	
	if (p != NULL && p->p_pendingcnt >= max_pending_per_proc) {
		signal_overflow++;
		ret = EAGAIN;
	} else if ((ksi = ksiginfo_alloc(0)) == NULL) {
		signal_alloc_fail++;
		ret = EAGAIN;
	} else {
		if (p != NULL)
			p->p_pendingcnt++;
		ksiginfo_copy(si, ksi);
		ksi->ksi_signo = signo;
		TAILQ_INSERT_TAIL(&sq->sq_list, ksi, ksi_link);
		ksi->ksi_sigq = sq;
	}

	if ((si->ksi_flags & KSI_TRAP) != 0 ||
	    (si->ksi_flags & KSI_SIGQ) == 0) {
		if (ret != 0)
			SIGADDSET(sq->sq_kill, signo);
		ret = 0;
		goto out_set_bit;
	}

	if (ret != 0)
		return (ret);
	
out_set_bit:
	SIGADDSET(sq->sq_signals, signo);
	return (ret);
}

void
sigqueue_flush(sigqueue_t *sq)
{
	struct proc *p = sq->sq_proc;
	ksiginfo_t *ksi;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));

	if (p != NULL)
		PROC_LOCK_ASSERT(p, MA_OWNED);

	while ((ksi = TAILQ_FIRST(&sq->sq_list)) != NULL) {
		TAILQ_REMOVE(&sq->sq_list, ksi, ksi_link);
		ksi->ksi_sigq = NULL;
		if (ksiginfo_tryfree(ksi) && p != NULL)
			p->p_pendingcnt--;
	}

	SIGEMPTYSET(sq->sq_signals);
	SIGEMPTYSET(sq->sq_kill);
}

void
sigqueue_collect_set(sigqueue_t *sq, sigset_t *set)
{
	ksiginfo_t *ksi;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));

	TAILQ_FOREACH(ksi, &sq->sq_list, ksi_link)
		SIGADDSET(*set, ksi->ksi_signo);
	SIGSETOR(*set, sq->sq_kill);
}

void
sigqueue_move_set(sigqueue_t *src, sigqueue_t *dst, sigset_t *setp)
{
	sigset_t tmp, set;
	struct proc *p1, *p2;
	ksiginfo_t *ksi, *next;

	KASSERT(src->sq_flags & SQ_INIT, ("src sigqueue not inited"));
	KASSERT(dst->sq_flags & SQ_INIT, ("dst sigqueue not inited"));
	/*
	 * make a copy, this allows setp to point to src or dst
	 * sq_signals without trouble.
	 */
	set = *setp;
	p1 = src->sq_proc;
	p2 = dst->sq_proc;
	/* Move siginfo to target list */
	TAILQ_FOREACH_SAFE(ksi, &src->sq_list, ksi_link, next) {
		if (SIGISMEMBER(set, ksi->ksi_signo)) {
			TAILQ_REMOVE(&src->sq_list, ksi, ksi_link);
			if (p1 != NULL)
				p1->p_pendingcnt--;
			TAILQ_INSERT_TAIL(&dst->sq_list, ksi, ksi_link);
			ksi->ksi_sigq = dst;
			if (p2 != NULL)
				p2->p_pendingcnt++;
		}
	}

	/* Move pending bits to target list */
	tmp = src->sq_kill;
	SIGSETAND(tmp, set);
	SIGSETOR(dst->sq_kill, tmp);
	SIGSETNAND(src->sq_kill, tmp);

	tmp = src->sq_signals;
	SIGSETAND(tmp, set);
	SIGSETOR(dst->sq_signals, tmp);
	SIGSETNAND(src->sq_signals, tmp);

	/* Finally, rescan src queue and set pending bits for it */
	sigqueue_collect_set(src, &src->sq_signals);
}

void
sigqueue_move(sigqueue_t *src, sigqueue_t *dst, int signo)
{
	sigset_t set;

	SIGEMPTYSET(set);
	SIGADDSET(set, signo);
	sigqueue_move_set(src, dst, &set);
}

void
sigqueue_delete_set(sigqueue_t *sq, sigset_t *set)
{
	struct proc *p = sq->sq_proc;
	ksiginfo_t *ksi, *next;

	KASSERT(sq->sq_flags & SQ_INIT, ("src sigqueue not inited"));

	/* Remove siginfo queue */
	TAILQ_FOREACH_SAFE(ksi, &sq->sq_list, ksi_link, next) {
		if (SIGISMEMBER(*set, ksi->ksi_signo)) {
			TAILQ_REMOVE(&sq->sq_list, ksi, ksi_link);
			ksi->ksi_sigq = NULL;
			if (ksiginfo_tryfree(ksi) && p != NULL)
				p->p_pendingcnt--;
		}
	}
	SIGSETNAND(sq->sq_kill, *set);
	SIGSETNAND(sq->sq_signals, *set);
	/* Finally, rescan queue and set pending bits for it */
	sigqueue_collect_set(sq, &sq->sq_signals);
}

void
sigqueue_delete(sigqueue_t *sq, int signo)
{
	sigset_t set;

	SIGEMPTYSET(set);
	SIGADDSET(set, signo);
	sigqueue_delete_set(sq, &set);
}

/* Remove a set of signals for a process */
void
sigqueue_delete_set_proc(struct proc *p, sigset_t *set)
{
	sigqueue_t worklist;
	struct thread *td0;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	sigqueue_init(&worklist, NULL);
	sigqueue_move_set(&p->p_sigqueue, &worklist, set);

	FOREACH_THREAD_IN_PROC(p, td0)
		sigqueue_move_set(&td0->td_sigqueue, &worklist, set);

	sigqueue_flush(&worklist);
}

void
sigqueue_delete_proc(struct proc *p, int signo)
{
	sigset_t set;

	SIGEMPTYSET(set);
	SIGADDSET(set, signo);
	sigqueue_delete_set_proc(p, &set);
}

void
sigqueue_delete_stopmask_proc(struct proc *p)
{
	sigset_t set;

	SIGEMPTYSET(set);
	SIGADDSET(set, SIGSTOP);
	SIGADDSET(set, SIGTSTP);
	SIGADDSET(set, SIGTTIN);
	SIGADDSET(set, SIGTTOU);
	sigqueue_delete_set_proc(p, &set);
}

/*
 * Determine signal that should be delivered to process p, the current
 * process, 0 if none.  If there is a pending stop signal with default
 * action, the process stops in issignal().
 */
int
cursig(struct thread *td, int stop_allowed)
{
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	KASSERT(stop_allowed == SIG_STOP_ALLOWED ||
	    stop_allowed == SIG_STOP_NOT_ALLOWED, ("cursig: stop_allowed"));
	mtx_assert(&td->td_proc->p_sigacts->ps_mtx, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_NOTOWNED);
	return (SIGPENDING(td) ? issignal(td, stop_allowed) : 0);
}

/*
 * Arrange for ast() to handle unmasked pending signals on return to user
 * mode.  This must be called whenever a signal is added to td_sigqueue or
 * unmasked in td_sigmask.
 */
void
signotify(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (SIGPENDING(td)) {
		thread_lock(td);
		td->td_flags |= TDF_NEEDSIGCHK | TDF_ASTPENDING;
		thread_unlock(td);
	}
}

int
sigonstack(size_t sp)
{
	struct thread *td = curthread;

	return ((td->td_pflags & TDP_ALTSTACK) ?
#if defined(COMPAT_43)
	    ((td->td_sigstk.ss_size == 0) ?
		(td->td_sigstk.ss_flags & SS_ONSTACK) :
		((sp - (size_t)td->td_sigstk.ss_sp) < td->td_sigstk.ss_size))
#else
	    ((sp - (size_t)td->td_sigstk.ss_sp) < td->td_sigstk.ss_size)
#endif
	    : 0);
}

static __inline int
sigprop(int sig)
{

	if (sig > 0 && sig < NSIG)
		return (sigproptbl[_SIG_IDX(sig)]);
	return (0);
}

int
sig_ffs(sigset_t *set)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++)
		if (set->__bits[i])
			return (ffs(set->__bits[i]) + (i * 32));
	return (0);
}

/*
 * kern_sigaction
 * sigaction
 * freebsd4_sigaction
 * osigaction
 */
int
kern_sigaction(td, sig, act, oact, flags)
	struct thread *td;
	register int sig;
	struct sigaction *act, *oact;
	int flags;
{
	struct sigacts *ps;
	struct proc *p = td->td_proc;

	if (!_SIG_VALID(sig))
		return (EINVAL);

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if (oact) {
		oact->sa_mask = ps->ps_catchmask[_SIG_IDX(sig)];
		oact->sa_flags = 0;
		if (SIGISMEMBER(ps->ps_sigonstack, sig))
			oact->sa_flags |= SA_ONSTACK;
		if (!SIGISMEMBER(ps->ps_sigintr, sig))
			oact->sa_flags |= SA_RESTART;
		if (SIGISMEMBER(ps->ps_sigreset, sig))
			oact->sa_flags |= SA_RESETHAND;
		if (SIGISMEMBER(ps->ps_signodefer, sig))
			oact->sa_flags |= SA_NODEFER;
		if (SIGISMEMBER(ps->ps_siginfo, sig)) {
			oact->sa_flags |= SA_SIGINFO;
			oact->sa_sigaction =
			    (__siginfohandler_t *)ps->ps_sigact[_SIG_IDX(sig)];
		} else
			oact->sa_handler = ps->ps_sigact[_SIG_IDX(sig)];
		if (sig == SIGCHLD && ps->ps_flag & PS_NOCLDSTOP)
			oact->sa_flags |= SA_NOCLDSTOP;
		if (sig == SIGCHLD && ps->ps_flag & PS_NOCLDWAIT)
			oact->sa_flags |= SA_NOCLDWAIT;
	}
	if (act) {
		if ((sig == SIGKILL || sig == SIGSTOP) &&
		    act->sa_handler != SIG_DFL) {
			mtx_unlock(&ps->ps_mtx);
			PROC_UNLOCK(p);
			return (EINVAL);
		}

		/*
		 * Change setting atomically.
		 */

		ps->ps_catchmask[_SIG_IDX(sig)] = act->sa_mask;
		SIG_CANTMASK(ps->ps_catchmask[_SIG_IDX(sig)]);
		if (act->sa_flags & SA_SIGINFO) {
			ps->ps_sigact[_SIG_IDX(sig)] =
			    (__sighandler_t *)act->sa_sigaction;
			SIGADDSET(ps->ps_siginfo, sig);
		} else {
			ps->ps_sigact[_SIG_IDX(sig)] = act->sa_handler;
			SIGDELSET(ps->ps_siginfo, sig);
		}
		if (!(act->sa_flags & SA_RESTART))
			SIGADDSET(ps->ps_sigintr, sig);
		else
			SIGDELSET(ps->ps_sigintr, sig);
		if (act->sa_flags & SA_ONSTACK)
			SIGADDSET(ps->ps_sigonstack, sig);
		else
			SIGDELSET(ps->ps_sigonstack, sig);
		if (act->sa_flags & SA_RESETHAND)
			SIGADDSET(ps->ps_sigreset, sig);
		else
			SIGDELSET(ps->ps_sigreset, sig);
		if (act->sa_flags & SA_NODEFER)
			SIGADDSET(ps->ps_signodefer, sig);
		else
			SIGDELSET(ps->ps_signodefer, sig);
		if (sig == SIGCHLD) {
			if (act->sa_flags & SA_NOCLDSTOP)
				ps->ps_flag |= PS_NOCLDSTOP;
			else
				ps->ps_flag &= ~PS_NOCLDSTOP;
			if (act->sa_flags & SA_NOCLDWAIT) {
				/*
				 * Paranoia: since SA_NOCLDWAIT is implemented
				 * by reparenting the dying child to PID 1 (and
				 * trust it to reap the zombie), PID 1 itself
				 * is forbidden to set SA_NOCLDWAIT.
				 */
				if (p->p_pid == 1)
					ps->ps_flag &= ~PS_NOCLDWAIT;
				else
					ps->ps_flag |= PS_NOCLDWAIT;
			} else
				ps->ps_flag &= ~PS_NOCLDWAIT;
			if (ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN)
				ps->ps_flag |= PS_CLDSIGIGN;
			else
				ps->ps_flag &= ~PS_CLDSIGIGN;
		}
		/*
		 * Set bit in ps_sigignore for signals that are set to SIG_IGN,
		 * and for signals set to SIG_DFL where the default is to
		 * ignore. However, don't put SIGCONT in ps_sigignore, as we
		 * have to restart the process.
		 */
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    (sigprop(sig) & SA_IGNORE &&
		     ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)) {
			/* never to be seen again */
			sigqueue_delete_proc(p, sig);
			if (sig != SIGCONT)
				/* easier in psignal */
				SIGADDSET(ps->ps_sigignore, sig);
			SIGDELSET(ps->ps_sigcatch, sig);
		} else {
			SIGDELSET(ps->ps_sigignore, sig);
			if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)
				SIGDELSET(ps->ps_sigcatch, sig);
			else
				SIGADDSET(ps->ps_sigcatch, sig);
		}
#ifdef COMPAT_FREEBSD4
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL ||
		    (flags & KSA_FREEBSD4) == 0)
			SIGDELSET(ps->ps_freebsd4, sig);
		else
			SIGADDSET(ps->ps_freebsd4, sig);
#endif
#ifdef COMPAT_43
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL ||
		    (flags & KSA_OSIGSET) == 0)
			SIGDELSET(ps->ps_osigset, sig);
		else
			SIGADDSET(ps->ps_osigset, sig);
#endif
	}
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct sigaction_args {
	int	sig;
	struct	sigaction *act;
	struct	sigaction *oact;
};
#endif
int
sigaction(td, uap)
	struct thread *td;
	register struct sigaction_args *uap;
{
	struct sigaction act, oact;
	register struct sigaction *actp, *oactp;
	int error;

	actp = (uap->act != NULL) ? &act : NULL;
	oactp = (uap->oact != NULL) ? &oact : NULL;
	if (actp) {
		error = copyin(uap->act, actp, sizeof(act));
		if (error)
			return (error);
	}
	error = kern_sigaction(td, uap->sig, actp, oactp, 0);
	if (oactp && !error)
		error = copyout(oactp, uap->oact, sizeof(oact));
	return (error);
}

#ifdef COMPAT_FREEBSD4
#ifndef _SYS_SYSPROTO_H_
struct freebsd4_sigaction_args {
	int	sig;
	struct	sigaction *act;
	struct	sigaction *oact;
};
#endif
int
freebsd4_sigaction(td, uap)
	struct thread *td;
	register struct freebsd4_sigaction_args *uap;
{
	struct sigaction act, oact;
	register struct sigaction *actp, *oactp;
	int error;


	actp = (uap->act != NULL) ? &act : NULL;
	oactp = (uap->oact != NULL) ? &oact : NULL;
	if (actp) {
		error = copyin(uap->act, actp, sizeof(act));
		if (error)
			return (error);
	}
	error = kern_sigaction(td, uap->sig, actp, oactp, KSA_FREEBSD4);
	if (oactp && !error)
		error = copyout(oactp, uap->oact, sizeof(oact));
	return (error);
}
#endif	/* COMAPT_FREEBSD4 */

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigaction_args {
	int	signum;
	struct	osigaction *nsa;
	struct	osigaction *osa;
};
#endif
int
osigaction(td, uap)
	struct thread *td;
	register struct osigaction_args *uap;
{
	struct osigaction sa;
	struct sigaction nsa, osa;
	register struct sigaction *nsap, *osap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);

	nsap = (uap->nsa != NULL) ? &nsa : NULL;
	osap = (uap->osa != NULL) ? &osa : NULL;

	if (nsap) {
		error = copyin(uap->nsa, &sa, sizeof(sa));
		if (error)
			return (error);
		nsap->sa_handler = sa.sa_handler;
		nsap->sa_flags = sa.sa_flags;
		OSIG2SIG(sa.sa_mask, nsap->sa_mask);
	}
	error = kern_sigaction(td, uap->signum, nsap, osap, KSA_OSIGSET);
	if (osap && !error) {
		sa.sa_handler = osap->sa_handler;
		sa.sa_flags = osap->sa_flags;
		SIG2OSIG(osap->sa_mask, sa.sa_mask);
		error = copyout(&sa, uap->osa, sizeof(sa));
	}
	return (error);
}

#if !defined(__i386__)
/* Avoid replicating the same stub everywhere */
int
osigreturn(td, uap)
	struct thread *td;
	struct osigreturn_args *uap;
{

	return (nosys(td, (struct nosys_args *)uap));
}
#endif
#endif /* COMPAT_43 */

/*
 * Initialize signal state for process 0;
 * set to ignore signals that are ignored by default.
 */
void
siginit(p)
	struct proc *p;
{
	register int i;
	struct sigacts *ps;

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	for (i = 1; i <= NSIG; i++)
		if (sigprop(i) & SA_IGNORE && i != SIGCONT)
			SIGADDSET(ps->ps_sigignore, i);
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps;
	int sig;
	struct thread *td;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through td_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);
	td = FIRST_THREAD_IN_PROC(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	while (SIGNOTEMPTY(ps->ps_sigcatch)) {
		sig = sig_ffs(&ps->ps_sigcatch);
		SIGDELSET(ps->ps_sigcatch, sig);
		if (sigprop(sig) & SA_IGNORE) {
			if (sig != SIGCONT)
				SIGADDSET(ps->ps_sigignore, sig);
			sigqueue_delete_proc(p, sig);
		}
		ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	td->td_sigstk.ss_flags = SS_DISABLE;
	td->td_sigstk.ss_size = 0;
	td->td_sigstk.ss_sp = 0;
	td->td_pflags &= ~TDP_ALTSTACK;
	/*
	 * Reset no zombies if child dies flag as Solaris does.
	 */
	ps->ps_flag &= ~(PS_NOCLDWAIT | PS_CLDSIGIGN);
	if (ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN)
		ps->ps_sigact[_SIG_IDX(SIGCHLD)] = SIG_DFL;
	mtx_unlock(&ps->ps_mtx);
}

/*
 * kern_sigprocmask()
 *
 *	Manipulate signal mask.
 */
int
kern_sigprocmask(struct thread *td, int how, sigset_t *set, sigset_t *oset,
    int flags)
{
	sigset_t new_block, oset1;
	struct proc *p;
	int error;

	p = td->td_proc;
	if (!(flags & SIGPROCMASK_PROC_LOCKED))
		PROC_LOCK(p);
	if (oset != NULL)
		*oset = td->td_sigmask;

	error = 0;
	SIGEMPTYSET(new_block);
	if (set != NULL) {
		switch (how) {
		case SIG_BLOCK:
			SIG_CANTMASK(*set);
			oset1 = td->td_sigmask;
			SIGSETOR(td->td_sigmask, *set);
			new_block = td->td_sigmask;
			SIGSETNAND(new_block, oset1);
			break;
		case SIG_UNBLOCK:
			SIGSETNAND(td->td_sigmask, *set);
			signotify(td);
			break;
		case SIG_SETMASK:
			SIG_CANTMASK(*set);
			oset1 = td->td_sigmask;
			if (flags & SIGPROCMASK_OLD)
				SIGSETLO(td->td_sigmask, *set);
			else
				td->td_sigmask = *set;
			new_block = td->td_sigmask;
			SIGSETNAND(new_block, oset1);
			signotify(td);
			break;
		default:
			error = EINVAL;
			break;
		}
	}

	/*
	 * The new_block set contains signals that were not previously
	 * blocked, but are blocked now.
	 *
	 * In case we block any signal that was not previously blocked
	 * for td, and process has the signal pending, try to schedule
	 * signal delivery to some thread that does not block the signal,
	 * possibly waking it up.
	 */
	if (p->p_numthreads != 1)
		reschedule_signals(p, new_block, flags);

	if (!(flags & SIGPROCMASK_PROC_LOCKED))
		PROC_UNLOCK(p);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct sigprocmask_args {
	int	how;
	const sigset_t *set;
	sigset_t *oset;
};
#endif
int
sigprocmask(td, uap)
	register struct thread *td;
	struct sigprocmask_args *uap;
{
	sigset_t set, oset;
	sigset_t *setp, *osetp;
	int error;

	setp = (uap->set != NULL) ? &set : NULL;
	osetp = (uap->oset != NULL) ? &oset : NULL;
	if (setp) {
		error = copyin(uap->set, setp, sizeof(set));
		if (error)
			return (error);
	}
	error = kern_sigprocmask(td, uap->how, setp, osetp, 0);
	if (osetp && !error) {
		error = copyout(osetp, uap->oset, sizeof(oset));
	}
	return (error);
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigprocmask_args {
	int	how;
	osigset_t mask;
};
#endif
int
osigprocmask(td, uap)
	register struct thread *td;
	struct osigprocmask_args *uap;
{
	sigset_t set, oset;
	int error;

	OSIG2SIG(uap->mask, set);
	error = kern_sigprocmask(td, uap->how, &set, &oset, 1);
	SIG2OSIG(oset, td->td_retval[0]);
	return (error);
}
#endif /* COMPAT_43 */

int
sigwait(struct thread *td, struct sigwait_args *uap)
{
	ksiginfo_t ksi;
	sigset_t set;
	int error;

	error = copyin(uap->set, &set, sizeof(set));
	if (error) {
		td->td_retval[0] = error;
		return (0);
	}

	error = kern_sigtimedwait(td, set, &ksi, NULL);
	if (error) {
		if (error == ERESTART)
			return (error);
		td->td_retval[0] = error;
		return (0);
	}

	error = copyout(&ksi.ksi_signo, uap->sig, sizeof(ksi.ksi_signo));
	td->td_retval[0] = error;
	return (0);
}

int
sigtimedwait(struct thread *td, struct sigtimedwait_args *uap)
{
	struct timespec ts;
	struct timespec *timeout;
	sigset_t set;
	ksiginfo_t ksi;
	int error;

	if (uap->timeout) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return (error);

		timeout = &ts;
	} else
		timeout = NULL;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &ksi, timeout);
	if (error)
		return (error);

	if (uap->info)
		error = copyout(&ksi.ksi_info, uap->info, sizeof(siginfo_t));

	if (error == 0)
		td->td_retval[0] = ksi.ksi_signo;
	return (error);
}

int
sigwaitinfo(struct thread *td, struct sigwaitinfo_args *uap)
{
	ksiginfo_t ksi;
	sigset_t set;
	int error;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &ksi, NULL);
	if (error)
		return (error);

	if (uap->info)
		error = copyout(&ksi.ksi_info, uap->info, sizeof(siginfo_t));
	
	if (error == 0)
		td->td_retval[0] = ksi.ksi_signo;
	return (error);
}

int
kern_sigtimedwait(struct thread *td, sigset_t waitset, ksiginfo_t *ksi,
	struct timespec *timeout)
{
	struct sigacts *ps;
	sigset_t savedmask;
	struct proc *p;
	int error, sig, hz, i, timevalid = 0;
	struct timespec rts, ets, ts;
	struct timeval tv;

	p = td->td_proc;
	error = 0;
	sig = 0;
	ets.tv_sec = 0;
	ets.tv_nsec = 0;
	SIG_CANTMASK(waitset);

	PROC_LOCK(p);
	ps = p->p_sigacts;
	savedmask = td->td_sigmask;
	if (timeout) {
		if (timeout->tv_nsec >= 0 && timeout->tv_nsec < 1000000000) {
			timevalid = 1;
			getnanouptime(&rts);
		 	ets = rts;
			timespecadd(&ets, timeout);
		}
	}

restart:
	for (i = 1; i <= _SIG_MAXSIG; ++i) {
		if (!SIGISMEMBER(waitset, i))
			continue;
		if (!SIGISMEMBER(td->td_sigqueue.sq_signals, i)) {
			if (SIGISMEMBER(p->p_sigqueue.sq_signals, i)) {
				sigqueue_move(&p->p_sigqueue,
					&td->td_sigqueue, i);
			} else
				continue;
		}

		SIGFILLSET(td->td_sigmask);
		SIG_CANTMASK(td->td_sigmask);
		SIGDELSET(td->td_sigmask, i);
		mtx_lock(&ps->ps_mtx);
		sig = cursig(td, SIG_STOP_ALLOWED);
		mtx_unlock(&ps->ps_mtx);
		if (sig)
			goto out;
		else {
			/*
			 * Because cursig() may have stopped current thread,
			 * after it is resumed, things may have already been 
			 * changed, it should rescan any pending signals.
			 */
			goto restart;
		}
	}

	if (error)
		goto out;

	/*
	 * POSIX says this must be checked after looking for pending
	 * signals.
	 */
	if (timeout) {
		if (!timevalid) {
			error = EINVAL;
			goto out;
		}
		getnanouptime(&rts);
		if (timespeccmp(&rts, &ets, >=)) {
			error = EAGAIN;
			goto out;
		}
		ts = ets;
		timespecsub(&ts, &rts);
		TIMESPEC_TO_TIMEVAL(&tv, &ts);
		hz = tvtohz(&tv);
	} else
		hz = 0;

	td->td_sigmask = savedmask;
	SIGSETNAND(td->td_sigmask, waitset);
	signotify(td);
	error = msleep(&ps, &p->p_mtx, PPAUSE|PCATCH, "sigwait", hz);
	if (timeout) {
		if (error == ERESTART) {
			/* timeout can not be restarted. */
			error = EINTR;
		} else if (error == EAGAIN) {
			/* will calculate timeout by ourself. */
			error = 0;
		}
	}
	goto restart;

out:
	td->td_sigmask = savedmask;
	signotify(td);
	if (sig) {
		ksiginfo_init(ksi);
		sigqueue_get(&td->td_sigqueue, sig, ksi);
		ksi->ksi_signo = sig;

		SDT_PROBE(proc, kernel, , signal_clear, sig, ksi, 0, 0, 0);
		
		if (ksi->ksi_code == SI_TIMER)
			itimer_accept(p, ksi->ksi_timerid, ksi);
		error = 0;

#ifdef KTRACE
		if (KTRPOINT(td, KTR_PSIG)) {
			sig_t action;

			mtx_lock(&ps->ps_mtx);
			action = ps->ps_sigact[_SIG_IDX(sig)];
			mtx_unlock(&ps->ps_mtx);
			ktrpsig(sig, action, &td->td_sigmask, 0);
		}
#endif
		if (sig == SIGKILL)
			sigexit(td, sig);
	}
	PROC_UNLOCK(p);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct sigpending_args {
	sigset_t	*set;
};
#endif
int
sigpending(td, uap)
	struct thread *td;
	struct sigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t pending;

	PROC_LOCK(p);
	pending = p->p_sigqueue.sq_signals;
	SIGSETOR(pending, td->td_sigqueue.sq_signals);
	PROC_UNLOCK(p);
	return (copyout(&pending, uap->set, sizeof(sigset_t)));
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigpending_args {
	int	dummy;
};
#endif
int
osigpending(td, uap)
	struct thread *td;
	struct osigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t pending;

	PROC_LOCK(p);
	pending = p->p_sigqueue.sq_signals;
	SIGSETOR(pending, td->td_sigqueue.sq_signals);
	PROC_UNLOCK(p);
	SIG2OSIG(pending, td->td_retval[0]);
	return (0);
}
#endif /* COMPAT_43 */

#if defined(COMPAT_43)
/*
 * Generalized interface signal handler, 4.3-compatible.
 */
#ifndef _SYS_SYSPROTO_H_
struct osigvec_args {
	int	signum;
	struct	sigvec *nsv;
	struct	sigvec *osv;
};
#endif
/* ARGSUSED */
int
osigvec(td, uap)
	struct thread *td;
	register struct osigvec_args *uap;
{
	struct sigvec vec;
	struct sigaction nsa, osa;
	register struct sigaction *nsap, *osap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);
	nsap = (uap->nsv != NULL) ? &nsa : NULL;
	osap = (uap->osv != NULL) ? &osa : NULL;
	if (nsap) {
		error = copyin(uap->nsv, &vec, sizeof(vec));
		if (error)
			return (error);
		nsap->sa_handler = vec.sv_handler;
		OSIG2SIG(vec.sv_mask, nsap->sa_mask);
		nsap->sa_flags = vec.sv_flags;
		nsap->sa_flags ^= SA_RESTART;	/* opposite of SV_INTERRUPT */
	}
	error = kern_sigaction(td, uap->signum, nsap, osap, KSA_OSIGSET);
	if (osap && !error) {
		vec.sv_handler = osap->sa_handler;
		SIG2OSIG(osap->sa_mask, vec.sv_mask);
		vec.sv_flags = osap->sa_flags;
		vec.sv_flags &= ~SA_NOCLDWAIT;
		vec.sv_flags ^= SA_RESTART;
		error = copyout(&vec, uap->osv, sizeof(vec));
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct osigblock_args {
	int	mask;
};
#endif
int
osigblock(td, uap)
	register struct thread *td;
	struct osigblock_args *uap;
{
	sigset_t set, oset;

	OSIG2SIG(uap->mask, set);
	kern_sigprocmask(td, SIG_BLOCK, &set, &oset, 0);
	SIG2OSIG(oset, td->td_retval[0]);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct osigsetmask_args {
	int	mask;
};
#endif
int
osigsetmask(td, uap)
	struct thread *td;
	struct osigsetmask_args *uap;
{
	sigset_t set, oset;

	OSIG2SIG(uap->mask, set);
	kern_sigprocmask(td, SIG_SETMASK, &set, &oset, 0);
	SIG2OSIG(oset, td->td_retval[0]);
	return (0);
}
#endif /* COMPAT_43 */

/*
 * Suspend calling thread until signal, providing mask to be set in the
 * meantime. 
 */
#ifndef _SYS_SYSPROTO_H_
struct sigsuspend_args {
	const sigset_t *sigmask;
};
#endif
/* ARGSUSED */
int
sigsuspend(td, uap)
	struct thread *td;
	struct sigsuspend_args *uap;
{
	sigset_t mask;
	int error;

	error = copyin(uap->sigmask, &mask, sizeof(mask));
	if (error)
		return (error);
	return (kern_sigsuspend(td, mask));
}

int
kern_sigsuspend(struct thread *td, sigset_t mask)
{
	struct proc *p = td->td_proc;
	int has_sig, sig;

	/*
	 * When returning from sigsuspend, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigacts structure
	 * to indicate this.
	 */
	PROC_LOCK(p);
	kern_sigprocmask(td, SIG_SETMASK, &mask, &td->td_oldsigmask,
	    SIGPROCMASK_PROC_LOCKED);
	td->td_pflags |= TDP_OLDMASK;

	/*
	 * Process signals now. Otherwise, we can get spurious wakeup
	 * due to signal entered process queue, but delivered to other
	 * thread. But sigsuspend should return only on signal
	 * delivery.
	 */
	cpu_set_syscall_retval(td, EINTR);
	for (has_sig = 0; !has_sig;) {
		while (msleep(&p->p_sigacts, &p->p_mtx, PPAUSE|PCATCH, "pause",
			0) == 0)
			/* void */;
		thread_suspend_check(0);
		mtx_lock(&p->p_sigacts->ps_mtx);
		while ((sig = cursig(td, SIG_STOP_ALLOWED)) != 0)
			has_sig += postsig(sig);
		mtx_unlock(&p->p_sigacts->ps_mtx);
	}
	PROC_UNLOCK(p);
	return (EJUSTRETURN);
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
/*
 * Compatibility sigsuspend call for old binaries.  Note nonstandard calling
 * convention: libc stub passes mask, not pointer, to save a copyin.
 */
#ifndef _SYS_SYSPROTO_H_
struct osigsuspend_args {
	osigset_t mask;
};
#endif
/* ARGSUSED */
int
osigsuspend(td, uap)
	struct thread *td;
	struct osigsuspend_args *uap;
{
	sigset_t mask;

	OSIG2SIG(uap->mask, mask);
	return (kern_sigsuspend(td, mask));
}
#endif /* COMPAT_43 */

#if defined(COMPAT_43)
#ifndef _SYS_SYSPROTO_H_
struct osigstack_args {
	struct	sigstack *nss;
	struct	sigstack *oss;
};
#endif
/* ARGSUSED */
int
osigstack(td, uap)
	struct thread *td;
	register struct osigstack_args *uap;
{
	struct sigstack nss, oss;
	int error = 0;

	if (uap->nss != NULL) {
		error = copyin(uap->nss, &nss, sizeof(nss));
		if (error)
			return (error);
	}
	oss.ss_sp = td->td_sigstk.ss_sp;
	oss.ss_onstack = sigonstack(cpu_getstack(td));
	if (uap->nss != NULL) {
		td->td_sigstk.ss_sp = nss.ss_sp;
		td->td_sigstk.ss_size = 0;
		td->td_sigstk.ss_flags |= nss.ss_onstack & SS_ONSTACK;
		td->td_pflags |= TDP_ALTSTACK;
	}
	if (uap->oss != NULL)
		error = copyout(&oss, uap->oss, sizeof(oss));

	return (error);
}
#endif /* COMPAT_43 */

#ifndef _SYS_SYSPROTO_H_
struct sigaltstack_args {
	stack_t	*ss;
	stack_t	*oss;
};
#endif
/* ARGSUSED */
int
sigaltstack(td, uap)
	struct thread *td;
	register struct sigaltstack_args *uap;
{
	stack_t ss, oss;
	int error;

	if (uap->ss != NULL) {
		error = copyin(uap->ss, &ss, sizeof(ss));
		if (error)
			return (error);
	}
	error = kern_sigaltstack(td, (uap->ss != NULL) ? &ss : NULL,
	    (uap->oss != NULL) ? &oss : NULL);
	if (error)
		return (error);
	if (uap->oss != NULL)
		error = copyout(&oss, uap->oss, sizeof(stack_t));
	return (error);
}

int
kern_sigaltstack(struct thread *td, stack_t *ss, stack_t *oss)
{
	struct proc *p = td->td_proc;
	int oonstack;

	oonstack = sigonstack(cpu_getstack(td));

	if (oss != NULL) {
		*oss = td->td_sigstk;
		oss->ss_flags = (td->td_pflags & TDP_ALTSTACK)
		    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	}

	if (ss != NULL) {
		if (oonstack)
			return (EPERM);
		if ((ss->ss_flags & ~SS_DISABLE) != 0)
			return (EINVAL);
		if (!(ss->ss_flags & SS_DISABLE)) {
			if (ss->ss_size < p->p_sysent->sv_minsigstksz)
				return (ENOMEM);

			td->td_sigstk = *ss;
			td->td_pflags |= TDP_ALTSTACK;
		} else {
			td->td_pflags &= ~TDP_ALTSTACK;
		}
	}
	return (0);
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
static int
killpg1(struct thread *td, int sig, int pgid, int all, ksiginfo_t *ksi)
{
	struct proc *p;
	struct pgrp *pgrp;
	int nfound = 0;

	if (all) {
		/*
		 * broadcast
		 */
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			PROC_LOCK(p);
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
			    p == td->td_proc || p->p_state == PRS_NEW) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p_cansignal(td, p, sig) == 0) {
				nfound++;
				if (sig)
					pksignal(p, sig, ksi);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
	} else {
		sx_slock(&proctree_lock);
		if (pgid == 0) {
			/*
			 * zero pgid means send to my process group.
			 */
			pgrp = td->td_proc->p_pgrp;
			PGRP_LOCK(pgrp);
		} else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL) {
				sx_sunlock(&proctree_lock);
				return (ESRCH);
			}
		}
		sx_sunlock(&proctree_lock);
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);	      
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
				p->p_state == PRS_NEW ) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p_cansignal(td, p, sig) == 0) {
				nfound++;
				if (sig)
					pksignal(p, sig, ksi);
			}
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(pgrp);
	}
	return (nfound ? 0 : ESRCH);
}

#ifndef _SYS_SYSPROTO_H_
struct kill_args {
	int	pid;
	int	signum;
};
#endif
/* ARGSUSED */
int
kill(struct thread *td, struct kill_args *uap)
{
	ksiginfo_t ksi;
	struct proc *p;
	int error;

	AUDIT_ARG_SIGNUM(uap->signum);
	AUDIT_ARG_PID(uap->pid);
	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	ksiginfo_init(&ksi);
	ksi.ksi_signo = uap->signum;
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_ucred->cr_ruid;

	if (uap->pid > 0) {
		/* kill single process */
		if ((p = pfind(uap->pid)) == NULL) {
			if ((p = zpfind(uap->pid)) == NULL)
				return (ESRCH);
		}
		AUDIT_ARG_PROCESS(p);
		error = p_cansignal(td, p, uap->signum);
		if (error == 0 && uap->signum)
			pksignal(p, uap->signum, &ksi);
		PROC_UNLOCK(p);
		return (error);
	}
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		return (killpg1(td, uap->signum, 0, 1, &ksi));
	case 0:			/* signal own process group */
		return (killpg1(td, uap->signum, 0, 0, &ksi));
	default:		/* negative explicit process group */
		return (killpg1(td, uap->signum, -uap->pid, 0, &ksi));
	}
	/* NOTREACHED */
}

#if defined(COMPAT_43)
#ifndef _SYS_SYSPROTO_H_
struct okillpg_args {
	int	pgid;
	int	signum;
};
#endif
/* ARGSUSED */
int
okillpg(struct thread *td, struct okillpg_args *uap)
{
	ksiginfo_t ksi;

	AUDIT_ARG_SIGNUM(uap->signum);
	AUDIT_ARG_PID(uap->pgid);
	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	ksiginfo_init(&ksi);
	ksi.ksi_signo = uap->signum;
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_ucred->cr_ruid;
	return (killpg1(td, uap->signum, uap->pgid, 0, &ksi));
}
#endif /* COMPAT_43 */

#ifndef _SYS_SYSPROTO_H_
struct sigqueue_args {
	pid_t pid;
	int signum;
	/* union sigval */ void *value;
};
#endif
int
sigqueue(struct thread *td, struct sigqueue_args *uap)
{
	ksiginfo_t ksi;
	struct proc *p;
	int error;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	/*
	 * Specification says sigqueue can only send signal to
	 * single process.
	 */
	if (uap->pid <= 0)
		return (EINVAL);

	if ((p = pfind(uap->pid)) == NULL) {
		if ((p = zpfind(uap->pid)) == NULL)
			return (ESRCH);
	}
	error = p_cansignal(td, p, uap->signum);
	if (error == 0 && uap->signum != 0) {
		ksiginfo_init(&ksi);
		ksi.ksi_flags = KSI_SIGQ;
		ksi.ksi_signo = uap->signum;
		ksi.ksi_code = SI_QUEUE;
		ksi.ksi_pid = td->td_proc->p_pid;
		ksi.ksi_uid = td->td_ucred->cr_ruid;
		ksi.ksi_value.sival_ptr = uap->value;
		error = tdsignal(p, NULL, ksi.ksi_signo, &ksi);
	}
	PROC_UNLOCK(p);
	return (error);
}

/*
 * Send a signal to a process group.
 */
void
gsignal(int pgid, int sig, ksiginfo_t *ksi)
{
	struct pgrp *pgrp;

	if (pgid != 0) {
		sx_slock(&proctree_lock);
		pgrp = pgfind(pgid);
		sx_sunlock(&proctree_lock);
		if (pgrp != NULL) {
			pgsignal(pgrp, sig, 0, ksi);
			PGRP_UNLOCK(pgrp);
		}
	}
}

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
pgsignal(struct pgrp *pgrp, int sig, int checkctty, ksiginfo_t *ksi)
{
	struct proc *p;

	if (pgrp) {
		PGRP_LOCK_ASSERT(pgrp, MA_OWNED);
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				pksignal(p, sig, ksi);
			PROC_UNLOCK(p);
		}
	}
}

/*
 * Send a signal caused by a trap to the current thread.  If it will be
 * caught immediately, deliver it with correct code.  Otherwise, post it
 * normally.
 */
void
trapsignal(struct thread *td, ksiginfo_t *ksi)
{
	struct sigacts *ps;
	sigset_t mask;
	struct proc *p;
	int sig;
	int code;

	p = td->td_proc;
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	KASSERT(_SIG_VALID(sig), ("invalid signal"));

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if ((p->p_flag & P_TRACED) == 0 && SIGISMEMBER(ps->ps_sigcatch, sig) &&
	    !SIGISMEMBER(td->td_sigmask, sig)) {
		td->td_ru.ru_nsignals++;
#ifdef KTRACE
		if (KTRPOINT(curthread, KTR_PSIG))
			ktrpsig(sig, ps->ps_sigact[_SIG_IDX(sig)],
			    &td->td_sigmask, code);
#endif
		(*p->p_sysent->sv_sendsig)(ps->ps_sigact[_SIG_IDX(sig)], 
				ksi, &td->td_sigmask);
		mask = ps->ps_catchmask[_SIG_IDX(sig)];
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(mask, sig);
		kern_sigprocmask(td, SIG_BLOCK, &mask, NULL,
		    SIGPROCMASK_PROC_LOCKED | SIGPROCMASK_PS_LOCKED);
		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See kern_sigaction() for origin of this code.
			 */
			SIGDELSET(ps->ps_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
		mtx_unlock(&ps->ps_mtx);
	} else {
		/*
		 * Avoid a possible infinite loop if the thread
		 * masking the signal or process is ignoring the
		 * signal.
		 */
		if (kern_forcesigexit &&
		    (SIGISMEMBER(td->td_sigmask, sig) ||
		     ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN)) {
			SIGDELSET(td->td_sigmask, sig);
			SIGDELSET(ps->ps_sigcatch, sig);
			SIGDELSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
		mtx_unlock(&ps->ps_mtx);
		p->p_code = code;	/* XXX for core dump/debugger */
		p->p_sig = sig;		/* XXX to verify code */
		tdsignal(p, td, sig, ksi);
	}
	PROC_UNLOCK(p);
}

static struct thread *
sigtd(struct proc *p, int sig, int prop)
{
	struct thread *td, *signal_td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * Check if current thread can handle the signal without
	 * switching context to another thread.
	 */
	if (curproc == p && !SIGISMEMBER(curthread->td_sigmask, sig))
		return (curthread);
	signal_td = NULL;
	FOREACH_THREAD_IN_PROC(p, td) {
		if (!SIGISMEMBER(td->td_sigmask, sig)) {
			signal_td = td;
			break;
		}
	}
	if (signal_td == NULL)
		signal_td = FIRST_THREAD_IN_PROC(p);
	return (signal_td);
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 * 
 * NB: This function may be entered from the debugger via the "kill" DDB
 * command.  There is little that can be done to mitigate the possibly messy
 * side effects of this unwise possibility.
 */
void
psignal(struct proc *p, int sig)
{
	ksiginfo_t ksi;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_KERNEL;
	(void) tdsignal(p, NULL, sig, &ksi);
}

void
pksignal(struct proc *p, int sig, ksiginfo_t *ksi)
{

	(void) tdsignal(p, NULL, sig, ksi);
}

int
psignal_event(struct proc *p, struct sigevent *sigev, ksiginfo_t *ksi)
{
	struct thread *td = NULL;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	KASSERT(!KSI_ONQ(ksi), ("psignal_event: ksi on queue"));

	/*
	 * ksi_code and other fields should be set before
	 * calling this function.
	 */
	ksi->ksi_signo = sigev->sigev_signo;
	ksi->ksi_value = sigev->sigev_value;
	if (sigev->sigev_notify == SIGEV_THREAD_ID) {
		td = thread_find(p, sigev->sigev_notify_thread_id);
		if (td == NULL)
			return (ESRCH);
	}
	return (tdsignal(p, td, ksi->ksi_signo, ksi));
}

int
tdsignal(struct proc *p, struct thread *td, int sig, ksiginfo_t *ksi)
{
	sig_t action;
	sigqueue_t *sigqueue;
	int prop;
	struct sigacts *ps;
	int intrval;
	int ret = 0;
	int wakeup_swapper;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (!_SIG_VALID(sig))
		panic("tdsignal(): invalid signal %d", sig);

	KASSERT(ksi == NULL || !KSI_ONQ(ksi), ("tdsignal: ksi on queue"));

	/*
	 * IEEE Std 1003.1-2001: return success when killing a zombie.
	 */
	if (p->p_state == PRS_ZOMBIE) {
		if (ksi && (ksi->ksi_flags & KSI_INS))
			ksiginfo_tryfree(ksi);
		return (ret);
	}

	ps = p->p_sigacts;
	KNOTE_LOCKED(&p->p_klist, NOTE_SIGNAL | sig);
	prop = sigprop(sig);

	if (td == NULL) {
		td = sigtd(p, sig, prop);
		sigqueue = &p->p_sigqueue;
	} else {
		KASSERT(td->td_proc == p, ("invalid thread"));
		sigqueue = &td->td_sigqueue;
	}

	SDT_PROBE(proc, kernel, , signal_send, td, p, sig, 0, 0 );

	/*
	 * If the signal is being ignored,
	 * then we forget about it immediately.
	 * (Note: we don't set SIGCONT in ps_sigignore,
	 * and if it is set to SIG_IGN,
	 * action will be SIG_DFL here.)
	 */
	mtx_lock(&ps->ps_mtx);
	if (SIGISMEMBER(ps->ps_sigignore, sig)) {
		SDT_PROBE(proc, kernel, , signal_discard, ps, td, sig, 0, 0 );

		mtx_unlock(&ps->ps_mtx);
		if (ksi && (ksi->ksi_flags & KSI_INS))
			ksiginfo_tryfree(ksi);
		return (ret);
	}
	if (SIGISMEMBER(td->td_sigmask, sig))
		action = SIG_HOLD;
	else if (SIGISMEMBER(ps->ps_sigcatch, sig))
		action = SIG_CATCH;
	else
		action = SIG_DFL;
	if (SIGISMEMBER(ps->ps_sigintr, sig))
		intrval = EINTR;
	else
		intrval = ERESTART;
	mtx_unlock(&ps->ps_mtx);

	if (prop & SA_CONT)
		sigqueue_delete_stopmask_proc(p);
	else if (prop & SA_STOP) {
		/*
		 * If sending a tty stop signal to a member of an orphaned
		 * process group, discard the signal here if the action
		 * is default; don't stop the process below if sleeping,
		 * and don't clear any pending SIGCONT.
		 */
		if ((prop & SA_TTYSTOP) &&
		    (p->p_pgrp->pg_jobc == 0) &&
		    (action == SIG_DFL)) {
			if (ksi && (ksi->ksi_flags & KSI_INS))
				ksiginfo_tryfree(ksi);
			return (ret);
		}
		sigqueue_delete_proc(p, SIGCONT);
		if (p->p_flag & P_CONTINUED) {
			p->p_flag &= ~P_CONTINUED;
			PROC_LOCK(p->p_pptr);
			sigqueue_take(p->p_ksi);
			PROC_UNLOCK(p->p_pptr);
		}
	}

	ret = sigqueue_add(sigqueue, sig, ksi);
	if (ret != 0)
		return (ret);
	signotify(td);
	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD &&
	    !((prop & SA_CONT) && (p->p_flag & P_STOPPED_SIG)))
		return (ret);
	/*
	 * SIGKILL: Remove procfs STOPEVENTs.
	 */
	if (sig == SIGKILL) {
		/* from procfs_ioctl.c: PIOCBIC */
		p->p_stops = 0;
		/* from procfs_ioctl.c: PIOCCONT */
		p->p_step = 0;
		wakeup(&p->p_step);
	}
	/*
	 * Some signals have a process-wide effect and a per-thread
	 * component.  Most processing occurs when the process next
	 * tries to cross the user boundary, however there are some
	 * times when processing needs to be done immediatly, such as
	 * waking up threads so that they can cross the user boundary.
	 * We try do the per-process part here.
	 */
	if (P_SHOULDSTOP(p)) {
		/*
		 * The process is in stopped mode. All the threads should be
		 * either winding down or already on the suspended queue.
		 */
		if (p->p_flag & P_TRACED) {
			/*
			 * The traced process is already stopped,
			 * so no further action is necessary.
			 * No signal can restart us.
			 */
			goto out;
		}

		if (sig == SIGKILL) {
			/*
			 * SIGKILL sets process running.
			 * It will die elsewhere.
			 * All threads must be restarted.
			 */
			p->p_flag &= ~P_STOPPED_SIG;
			goto runfast;
		}

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in sigqueue as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * sigqueue.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			p->p_flag &= ~P_STOPPED_SIG;
			PROC_SLOCK(p);
			if (p->p_numthreads == p->p_suspcount) {
				PROC_SUNLOCK(p);
				p->p_flag |= P_CONTINUED;
				p->p_xstat = SIGCONT;
				PROC_LOCK(p->p_pptr);
				childproc_continued(p);
				PROC_UNLOCK(p->p_pptr);
				PROC_SLOCK(p);
			}
			if (action == SIG_DFL) {
				thread_unsuspend(p);
				PROC_SUNLOCK(p);
				sigqueue_delete(sigqueue, sig);
				goto out;
			}
			if (action == SIG_CATCH) {
				/*
				 * The process wants to catch it so it needs
				 * to run at least one thread, but which one?
				 */
				PROC_SUNLOCK(p);
				goto runfast;
			}
			/*
			 * The signal is not ignored or caught.
			 */
			thread_unsuspend(p);
			PROC_SUNLOCK(p);
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again
			 * (If we did the shell could get confused).
			 * Just make sure the signal STOP bit set.
			 */
			p->p_flag |= P_STOPPED_SIG;
			sigqueue_delete(sigqueue, sig);
			goto out;
		}

		/*
		 * All other kinds of signals:
		 * If a thread is sleeping interruptibly, simulate a
		 * wakeup so that when it is continued it will be made
		 * runnable and can look at the signal.  However, don't make
		 * the PROCESS runnable, leave it stopped.
		 * It may run a bit until it hits a thread_suspend_check().
		 */
		wakeup_swapper = 0;
		PROC_SLOCK(p);
		thread_lock(td);
		if (TD_ON_SLEEPQ(td) && (td->td_flags & TDF_SINTR))
			wakeup_swapper = sleepq_abort(td, intrval);
		thread_unlock(td);
		PROC_SUNLOCK(p);
		if (wakeup_swapper)
			kick_proc0();
		goto out;
		/*
		 * Mutexes are short lived. Threads waiting on them will
		 * hit thread_suspend_check() soon.
		 */
	} else if (p->p_state == PRS_NORMAL) {
		if (p->p_flag & P_TRACED || action == SIG_CATCH) {
			tdsigwakeup(td, sig, action, intrval);
			goto out;
		}

		MPASS(action == SIG_DFL);

		if (prop & SA_STOP) {
			if (p->p_flag & P_PPWAIT)
				goto out;
			p->p_flag |= P_STOPPED_SIG;
			p->p_xstat = sig;
			PROC_SLOCK(p);
			sig_suspend_threads(td, p, 1);
			if (p->p_numthreads == p->p_suspcount) {
				/*
				 * only thread sending signal to another
				 * process can reach here, if thread is sending
				 * signal to its process, because thread does
				 * not suspend itself here, p_numthreads
				 * should never be equal to p_suspcount.
				 */
				thread_stopped(p);
				PROC_SUNLOCK(p);
				sigqueue_delete_proc(p, p->p_xstat);
			} else
				PROC_SUNLOCK(p);
			goto out;
		}
	} else {
		/* Not in "NORMAL" state. discard the signal. */
		sigqueue_delete(sigqueue, sig);
		goto out;
	}

	/*
	 * The process is not stopped so we need to apply the signal to all the
	 * running threads.
	 */
runfast:
	tdsigwakeup(td, sig, action, intrval);
	PROC_SLOCK(p);
	thread_unsuspend(p);
	PROC_SUNLOCK(p);
out:
	/* If we jump here, proc slock should not be owned. */
	PROC_SLOCK_ASSERT(p, MA_NOTOWNED);
	return (ret);
}

/*
 * The force of a signal has been directed against a single
 * thread.  We need to see what we can do about knocking it
 * out of any sleep it may be in etc.
 */
static void
tdsigwakeup(struct thread *td, int sig, sig_t action, int intrval)
{
	struct proc *p = td->td_proc;
	register int prop;
	int wakeup_swapper;

	wakeup_swapper = 0;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	prop = sigprop(sig);

	PROC_SLOCK(p);
	thread_lock(td);
	/*
	 * Bring the priority of a thread up if we want it to get
	 * killed in this lifetime.
	 */
	if (action == SIG_DFL && (prop & SA_KILL) && td->td_priority > PUSER)
		sched_prio(td, PUSER);
	if (TD_ON_SLEEPQ(td)) {
		/*
		 * If thread is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((td->td_flags & TDF_SINTR) == 0)
			goto out;
		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) && action == SIG_DFL) {
			thread_unlock(td);
			PROC_SUNLOCK(p);
			sigqueue_delete(&p->p_sigqueue, sig);
			/*
			 * It may be on either list in this state.
			 * Remove from both for now.
			 */
			sigqueue_delete(&td->td_sigqueue, sig);
			return;
		}

		/*
		 * Give low priority threads a better chance to run.
		 */
		if (td->td_priority > PUSER)
			sched_prio(td, PUSER);

		wakeup_swapper = sleepq_abort(td, intrval);
	} else {
		/*
		 * Other states do nothing with the signal immediately,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
#ifdef SMP
		if (TD_IS_RUNNING(td) && td != curthread)
			forward_signal(td);
#endif
	}
out:
	PROC_SUNLOCK(p);
	thread_unlock(td);
	if (wakeup_swapper)
		kick_proc0();
}

static void
sig_suspend_threads(struct thread *td, struct proc *p, int sending)
{
	struct thread *td2;
	int wakeup_swapper;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);

	wakeup_swapper = 0;
	FOREACH_THREAD_IN_PROC(p, td2) {
		thread_lock(td2);
		td2->td_flags |= TDF_ASTPENDING | TDF_NEEDSUSPCHK;
		if ((TD_IS_SLEEPING(td2) || TD_IS_SWAPPED(td2)) &&
		    (td2->td_flags & TDF_SINTR)) {
			if (td2->td_flags & TDF_SBDRY) {
				if (TD_IS_SUSPENDED(td2))
					wakeup_swapper |=
					    thread_unsuspend_one(td2);
				if (TD_ON_SLEEPQ(td2))
					wakeup_swapper |=
					    sleepq_abort(td2, ERESTART);
			} else if (!TD_IS_SUSPENDED(td2)) {
				thread_suspend_one(td2);
			}
		} else if (!TD_IS_SUSPENDED(td2)) {
			if (sending || td != td2)
				td2->td_flags |= TDF_ASTPENDING;
#ifdef SMP
			if (TD_IS_RUNNING(td2) && td2 != td)
				forward_signal(td2);
#endif
		}
		thread_unlock(td2);
	}
	if (wakeup_swapper)
		kick_proc0();
}

int
ptracestop(struct thread *td, int sig)
{
	struct proc *p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
	    &p->p_mtx.lock_object, "Stopping for traced signal");

	td->td_dbgflags |= TDB_XSIG;
	td->td_xsig = sig;
	PROC_SLOCK(p);
	while ((p->p_flag & P_TRACED) && (td->td_dbgflags & TDB_XSIG)) {
		if (p->p_flag & P_SINGLE_EXIT) {
			td->td_dbgflags &= ~TDB_XSIG;
			PROC_SUNLOCK(p);
			return (sig);
		}
		/*
		 * Just make wait() to work, the last stopped thread
		 * will win.
		 */
		p->p_xstat = sig;
		p->p_xthread = td;
		p->p_flag |= (P_STOPPED_SIG|P_STOPPED_TRACE);
		sig_suspend_threads(td, p, 0);
stopme:
		thread_suspend_switch(td);
		if (!(p->p_flag & P_TRACED)) {
			break;
		}
		if (td->td_dbgflags & TDB_SUSPEND) {
			if (p->p_flag & P_SINGLE_EXIT)
				break;
			goto stopme;
		}
	}
	PROC_SUNLOCK(p);
	return (td->td_xsig);
}

static void
reschedule_signals(struct proc *p, sigset_t block, int flags)
{
	struct sigacts *ps;
	struct thread *td;
	int i;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	ps = p->p_sigacts;
	for (i = 1; !SIGISEMPTY(block); i++) {
		if (!SIGISMEMBER(block, i))
			continue;
		SIGDELSET(block, i);
		if (!SIGISMEMBER(p->p_siglist, i))
			continue;

		td = sigtd(p, i, 0);
		signotify(td);
		if (!(flags & SIGPROCMASK_PS_LOCKED))
			mtx_lock(&ps->ps_mtx);
		if (p->p_flag & P_TRACED || SIGISMEMBER(ps->ps_sigcatch, i))
			tdsigwakeup(td, i, SIG_CATCH,
			    (SIGISMEMBER(ps->ps_sigintr, i) ? EINTR :
			     ERESTART));
		if (!(flags & SIGPROCMASK_PS_LOCKED))
			mtx_unlock(&ps->ps_mtx);
	}
}

void
tdsigcleanup(struct thread *td)
{
	struct proc *p;
	sigset_t unblocked;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	sigqueue_flush(&td->td_sigqueue);
	if (p->p_numthreads == 1)
		return;

	/*
	 * Since we cannot handle signals, notify signal post code
	 * about this by filling the sigmask.
	 *
	 * Also, if needed, wake up thread(s) that do not block the
	 * same signals as the exiting thread, since the thread might
	 * have been selected for delivery and woken up.
	 */
	SIGFILLSET(unblocked);
	SIGSETNAND(unblocked, td->td_sigmask);
	SIGFILLSET(td->td_sigmask);
	reschedule_signals(p, unblocked, 0);

}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling issignal
 * by checking the pending signal masks in cursig.) The normal call
 * sequence is
 *
 *	while (sig = cursig(curthread))
 *		postsig(sig);
 */
static int
issignal(struct thread *td, int stop_allowed)
{
	struct proc *p;
	struct sigacts *ps;
	struct sigqueue *queue;
	sigset_t sigpending;
	int sig, prop, newsig, signo;

	p = td->td_proc;
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	for (;;) {
		int traced = (p->p_flag & P_TRACED) || (p->p_stops & S_SIG);

		sigpending = td->td_sigqueue.sq_signals;
		SIGSETOR(sigpending, p->p_sigqueue.sq_signals);
		SIGSETNAND(sigpending, td->td_sigmask);

		if (p->p_flag & P_PPWAIT)
			SIG_STOPSIGMASK(sigpending);
		if (SIGISEMPTY(sigpending))	/* no signal to send */
			return (0);
		sig = sig_ffs(&sigpending);

		if (p->p_stops & S_SIG) {
			mtx_unlock(&ps->ps_mtx);
			stopevent(p, S_SIG, sig);
			mtx_lock(&ps->ps_mtx);
		}

		/*
		 * We should see pending but ignored signals
		 * only if P_TRACED was on when they were posted.
		 */
		if (SIGISMEMBER(ps->ps_sigignore, sig) && (traced == 0)) {
			sigqueue_delete(&td->td_sigqueue, sig);
			sigqueue_delete(&p->p_sigqueue, sig);
			continue;
		}
		if (p->p_flag & P_TRACED && (p->p_flag & P_PPWAIT) == 0) {
			/*
			 * If traced, always stop.
			 */
			mtx_unlock(&ps->ps_mtx);
			newsig = ptracestop(td, sig);
			mtx_lock(&ps->ps_mtx);

			if (sig != newsig) {
				ksiginfo_t ksi;

				queue = &td->td_sigqueue;
				/*
				 * clear old signal.
				 * XXX shrug off debugger, it causes siginfo to
				 * be thrown away.
				 */
				if (sigqueue_get(queue, sig, &ksi) == 0) {
					queue = &p->p_sigqueue;
					signo = sigqueue_get(queue, sig, &ksi);
					KASSERT(signo == sig, ("signo != sig"));
				}

				/*
				 * If parent wants us to take the signal,
				 * then it will leave it in p->p_xstat;
				 * otherwise we just look for signals again.
			 	*/
				if (newsig == 0)
					continue;
				sig = newsig;

				/*
				 * Put the new signal into td_sigqueue. If the
				 * signal is being masked, look for other signals.
				 */
				SIGADDSET(queue->sq_signals, sig);
				if (SIGISMEMBER(td->td_sigmask, sig))
					continue;
				signotify(td);
			}

			/*
			 * If the traced bit got turned off, go back up
			 * to the top to rescan signals.  This ensures
			 * that p_sig* and p_sigact are consistent.
			 */
			if ((p->p_flag & P_TRACED) == 0)
				continue;
		}

		prop = sigprop(sig);

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((intptr_t)p->p_sigacts->ps_sigact[_SIG_IDX(sig)]) {

		case (intptr_t)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf("Process (pid %lu) got signal %d\n",
					(u_long)p->p_pid, sig);
#endif
				break;		/* == ignore */
			}
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (p->p_flag & P_TRACED ||
		    		    (p->p_pgrp->pg_jobc == 0 &&
				     prop & SA_TTYSTOP))
					break;	/* == ignore */

				/* Ignore, but do not drop the stop signal. */
				if (stop_allowed != SIG_STOP_ALLOWED)
					return (sig);
				mtx_unlock(&ps->ps_mtx);
				WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
				    &p->p_mtx.lock_object, "Catching SIGSTOP");
				p->p_flag |= P_STOPPED_SIG;
				p->p_xstat = sig;
				PROC_SLOCK(p);
				sig_suspend_threads(td, p, 0);
				thread_suspend_switch(td);
				PROC_SUNLOCK(p);
				mtx_lock(&ps->ps_mtx);
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				return (sig);
			/*NOTREACHED*/

		case (intptr_t)SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (p->p_flag & P_TRACED) == 0)
				printf("issignal\n");
			break;		/* == ignore */

		default:
			/*
			 * This signal has an action, let
			 * postsig() process it.
			 */
			return (sig);
		}
		sigqueue_delete(&td->td_sigqueue, sig);		/* take the signal! */
		sigqueue_delete(&p->p_sigqueue, sig);
	}
	/* NOTREACHED */
}

void
thread_stopped(struct proc *p)
{
	int n;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	n = p->p_suspcount;
	if (p == curproc)
		n++;
	if ((p->p_flag & P_STOPPED_SIG) && (n == p->p_numthreads)) {
		PROC_SUNLOCK(p);
		p->p_flag &= ~P_WAITED;
		PROC_LOCK(p->p_pptr);
		childproc_stopped(p, (p->p_flag & P_TRACED) ?
			CLD_TRAPPED : CLD_STOPPED);
		PROC_UNLOCK(p->p_pptr);
		PROC_SLOCK(p);
	}
}
 
/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
int
postsig(sig)
	register int sig;
{
	struct thread *td = curthread;
	register struct proc *p = td->td_proc;
	struct sigacts *ps;
	sig_t action;
	ksiginfo_t ksi;
	sigset_t returnmask, mask;

	KASSERT(sig != 0, ("postsig"));

	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	ksiginfo_init(&ksi);
	if (sigqueue_get(&td->td_sigqueue, sig, &ksi) == 0 &&
	    sigqueue_get(&p->p_sigqueue, sig, &ksi) == 0)
		return (0);
	ksi.ksi_signo = sig;
	if (ksi.ksi_code == SI_TIMER)
		itimer_accept(p, ksi.ksi_timerid, &ksi);
	action = ps->ps_sigact[_SIG_IDX(sig)];
#ifdef KTRACE
	if (KTRPOINT(td, KTR_PSIG))
		ktrpsig(sig, action, td->td_pflags & TDP_OLDMASK ?
		    &td->td_oldsigmask : &td->td_sigmask, 0);
#endif
	if (p->p_stops & S_SIG) {
		mtx_unlock(&ps->ps_mtx);
		stopevent(p, S_SIG, sig);
		mtx_lock(&ps->ps_mtx);
	}

	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		mtx_unlock(&ps->ps_mtx);
		sigexit(td, sig);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
		KASSERT(action != SIG_IGN && !SIGISMEMBER(td->td_sigmask, sig),
		    ("postsig action"));
		/*
		 * Set the new mask value and also defer further
		 * occurrences of this signal.
		 *
		 * Special case: user has done a sigsuspend.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigsuspend is what we want
		 * restored after the signal processing is completed.
		 */
		if (td->td_pflags & TDP_OLDMASK) {
			returnmask = td->td_oldsigmask;
			td->td_pflags &= ~TDP_OLDMASK;
		} else
			returnmask = td->td_sigmask;

		mask = ps->ps_catchmask[_SIG_IDX(sig)];
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(mask, sig);
		kern_sigprocmask(td, SIG_BLOCK, &mask, NULL,
		    SIGPROCMASK_PROC_LOCKED | SIGPROCMASK_PS_LOCKED);

		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See kern_sigaction() for origin of this code.
			 */
			SIGDELSET(ps->ps_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
		td->td_ru.ru_nsignals++;
		if (p->p_sig == sig) {
			p->p_code = 0;
			p->p_sig = 0;
		}
		(*p->p_sysent->sv_sendsig)(action, &ksi, &returnmask);
	}
	return (1);
}

/*
 * Kill the current process for stated reason.
 */
void
killproc(p, why)
	struct proc *p;
	char *why;
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	CTR3(KTR_PROC, "killproc: proc %p (pid %d, %s)",
		p, p->p_pid, p->p_comm);
	log(LOG_ERR, "pid %d (%s), uid %d, was killed: %s\n", p->p_pid, p->p_comm,
		p->p_ucred ? p->p_ucred->cr_uid : -1, why);
	psignal(p, SIGKILL);
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 */
void
sigexit(td, sig)
	struct thread *td;
	int sig;
{
	struct proc *p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_acflag |= AXSIG;
	/*
	 * We must be single-threading to generate a core dump.  This
	 * ensures that the registers in the core file are up-to-date.
	 * Also, the ELF dump handler assumes that the thread list doesn't
	 * change out from under it.
	 *
	 * XXX If another thread attempts to single-thread before us
	 *     (e.g. via fork()), we won't get a dump at all.
	 */
	if ((sigprop(sig) & SA_CORE) && (thread_single(SINGLE_NO_EXIT) == 0)) {
		p->p_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 * Note that coredump() drops proc lock.
		 */
		if (coredump(td) == 0)
			sig |= WCOREFLAG;
		if (kern_logsigexit)
			log(LOG_INFO,
			    "pid %d (%s), uid %d: exited on signal %d%s\n",
			    p->p_pid, p->p_comm,
			    td->td_ucred ? td->td_ucred->cr_uid : -1,
			    sig &~ WCOREFLAG,
			    sig & WCOREFLAG ? " (core dumped)" : "");
	} else
		PROC_UNLOCK(p);
	exit1(td, W_EXITCODE(0, sig));
	/* NOTREACHED */
}

/*
 * Send queued SIGCHLD to parent when child process's state
 * is changed.
 */
static void
sigparent(struct proc *p, int reason, int status)
{
	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_LOCK_ASSERT(p->p_pptr, MA_OWNED);

	if (p->p_ksi != NULL) {
		p->p_ksi->ksi_signo  = SIGCHLD;
		p->p_ksi->ksi_code   = reason;
		p->p_ksi->ksi_status = status;
		p->p_ksi->ksi_pid    = p->p_pid;
		p->p_ksi->ksi_uid    = p->p_ucred->cr_ruid;
		if (KSI_ONQ(p->p_ksi))
			return;
	}
	tdsignal(p->p_pptr, NULL, SIGCHLD, p->p_ksi);
}

static void
childproc_jobstate(struct proc *p, int reason, int status)
{
	struct sigacts *ps;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_LOCK_ASSERT(p->p_pptr, MA_OWNED);

	/*
	 * Wake up parent sleeping in kern_wait(), also send
	 * SIGCHLD to parent, but SIGCHLD does not guarantee
	 * that parent will awake, because parent may masked
	 * the signal.
	 */
	p->p_pptr->p_flag |= P_STATCHILD;
	wakeup(p->p_pptr);

	ps = p->p_pptr->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if ((ps->ps_flag & PS_NOCLDSTOP) == 0) {
		mtx_unlock(&ps->ps_mtx);
		sigparent(p, reason, status);
	} else
		mtx_unlock(&ps->ps_mtx);
}

void
childproc_stopped(struct proc *p, int reason)
{
	childproc_jobstate(p, reason, p->p_xstat);
}

void
childproc_continued(struct proc *p)
{
	childproc_jobstate(p, CLD_CONTINUED, SIGCONT);
}

void
childproc_exited(struct proc *p)
{
	int reason;
	int status = p->p_xstat; /* convert to int */

	reason = CLD_EXITED;
	if (WCOREDUMP(status))
		reason = CLD_DUMPED;
	else if (WIFSIGNALED(status))
		reason = CLD_KILLED;
	/*
	 * XXX avoid calling wakeup(p->p_pptr), the work is
	 * done in exit1().
	 */
	sigparent(p, reason, status);
}

static char corefilename[MAXPATHLEN] = {"%N.core"};
SYSCTL_STRING(_kern, OID_AUTO, corefile, CTLFLAG_RW, corefilename,
	      sizeof(corefilename), "process corefile name format string");

/*
 * expand_name(name, uid, pid)
 * Expand the name described in corefilename, using name, uid, and pid.
 * corefilename is a printf-like string, with three format specifiers:
 *	%N	name of process ("name")
 *	%P	process id (pid)
 *	%U	user id (uid)
 * For example, "%N.core" is the default; they can be disabled completely
 * by using "/dev/null", or all core files can be stored in "/cores/%U/%N-%P".
 * This is controlled by the sysctl variable kern.corefile (see above).
 */
static char *
expand_name(name, uid, pid)
	const char *name;
	uid_t uid;
	pid_t pid;
{
	struct sbuf sb;
	const char *format;
	char *temp;
	size_t i;

	format = corefilename;
	temp = malloc(MAXPATHLEN, M_TEMP, M_NOWAIT | M_ZERO);
	if (temp == NULL)
		return (NULL);
	(void)sbuf_new(&sb, temp, MAXPATHLEN, SBUF_FIXEDLEN);
	for (i = 0; format[i]; i++) {
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				sbuf_putc(&sb, '%');
				break;
			case 'N':	/* process name */
				sbuf_printf(&sb, "%s", name);
				break;
			case 'P':	/* process id */
				sbuf_printf(&sb, "%u", pid);
				break;
			case 'U':	/* user id */
				sbuf_printf(&sb, "%u", uid);
				break;
			default:
			  	log(LOG_ERR,
				    "Unknown format character %c in "
				    "corename `%s'\n", format[i], format);
			}
			break;
		default:
			sbuf_putc(&sb, format[i]);
		}
	}
	if (sbuf_overflowed(&sb)) {
		sbuf_delete(&sb);
		log(LOG_ERR, "pid %ld (%s), uid (%lu): corename is too "
		    "long\n", (long)pid, name, (u_long)uid);
		free(temp, M_TEMP);
		return (NULL);
	}
	sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (temp);
}

/*
 * Dump a process' core.  The main routine does some
 * policy checking, and creates the name of the coredump;
 * then it passes on a vnode and a size limit to the process-specific
 * coredump routine if there is one; if there _is not_ one, it returns
 * ENOSYS; otherwise it returns the error from the process-specific routine.
 */

static int
coredump(struct thread *td)
{
	struct proc *p = td->td_proc;
	register struct vnode *vp;
	register struct ucred *cred = td->td_ucred;
	struct flock lf;
	struct nameidata nd;
	struct vattr vattr;
	int error, error1, flags, locked;
	struct mount *mp;
	char *name;			/* name of corefile */
	off_t limit;
	int vfslocked;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS((p->p_flag & P_HADTHREADS) == 0 || p->p_singlethread == td);
	_STOPEVENT(p, S_CORE, 0);

	name = expand_name(p->p_comm, td->td_ucred->cr_uid, p->p_pid);
	if (name == NULL) {
		PROC_UNLOCK(p);
#ifdef AUDIT
		audit_proc_coredump(td, NULL, EINVAL);
#endif
		return (EINVAL);
	}
	if (((sugid_coredump == 0) && p->p_flag & P_SUGID) || do_coredump == 0) {
		PROC_UNLOCK(p);
#ifdef AUDIT
		audit_proc_coredump(td, name, EFAULT);
#endif
		free(name, M_TEMP);
		return (EFAULT);
	}
	
	/*
	 * Note that the bulk of limit checking is done after
	 * the corefile is created.  The exception is if the limit
	 * for corefiles is 0, in which case we don't bother
	 * creating the corefile at all.  This layout means that
	 * a corefile is truncated instead of not being created,
	 * if it is larger than the limit.
	 */
	limit = (off_t)lim_cur(p, RLIMIT_CORE);
	PROC_UNLOCK(p);
	if (limit == 0) {
#ifdef AUDIT
		audit_proc_coredump(td, name, EFBIG);
#endif
		free(name, M_TEMP);
		return (EFBIG);
	}

restart:
	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE, UIO_SYSSPACE, name, td);
	flags = O_CREAT | FWRITE | O_NOFOLLOW;
	error = vn_open_cred(&nd, &flags, S_IRUSR | S_IWUSR, VN_OPEN_NOAUDIT,
	    cred, NULL);
	if (error) {
#ifdef AUDIT
		audit_proc_coredump(td, name, error);
#endif
		free(name, M_TEMP);
		return (error);
	}
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	/* Don't dump to non-regular files or files with links. */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred) || vattr.va_nlink != 1) {
		VOP_UNLOCK(vp, 0);
		error = EFAULT;
		goto close;
	}

	VOP_UNLOCK(vp, 0);
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	lf.l_type = F_WRLCK;
	locked = (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &lf, F_FLOCK) == 0);

	if (vn_start_write(vp, &mp, V_NOWAIT) != 0) {
		lf.l_type = F_UNLCK;
		if (locked)
			VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
		if ((error = vn_close(vp, FWRITE, cred, td)) != 0)
			goto out;
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			goto out;
		VFS_UNLOCK_GIANT(vfslocked);
		goto restart;
	}

	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	if (set_core_nodump_flag)
		vattr.va_flags = UF_NODUMP;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_SETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp, 0);
	vn_finished_write(mp);
	PROC_LOCK(p);
	p->p_acflag |= ACORE;
	PROC_UNLOCK(p);

	error = p->p_sysent->sv_coredump ?
	  p->p_sysent->sv_coredump(td, vp, limit) :
	  ENOSYS;

	if (locked) {
		lf.l_type = F_UNLCK;
		VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
	}
close:
	error1 = vn_close(vp, FWRITE, cred, td);
	if (error == 0)
		error = error1;
out:
#ifdef AUDIT
	audit_proc_coredump(td, name, error);
#endif
	free(name, M_TEMP);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Nonexistent system call-- signal process (may want to handle it).  Flag
 * error in case process won't see signal immediately (blocked or ignored).
 */
#ifndef _SYS_SYSPROTO_H_
struct nosys_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
nosys(td, args)
	struct thread *td;
	struct nosys_args *args;
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	psignal(p, SIGSYS);
	PROC_UNLOCK(p);
	return (ENOSYS);
}

/*
 * Send a SIGIO or SIGURG signal to a process or process group using stored
 * credentials rather than those of the current process.
 */
void
pgsigio(sigiop, sig, checkctty)
	struct sigio **sigiop;
	int sig, checkctty;
{
	ksiginfo_t ksi;
	struct sigio *sigio;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_KERNEL;

	SIGIO_LOCK();
	sigio = *sigiop;
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}
	if (sigio->sio_pgid > 0) {
		PROC_LOCK(sigio->sio_proc);
		if (CANSIGIO(sigio->sio_ucred, sigio->sio_proc->p_ucred))
			psignal(sigio->sio_proc, sig);
		PROC_UNLOCK(sigio->sio_proc);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		PGRP_LOCK(sigio->sio_pgrp);
		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (CANSIGIO(sigio->sio_ucred, p->p_ucred) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(sigio->sio_pgrp);
	}
	SIGIO_UNLOCK();
}

static int
filt_sigattach(struct knote *kn)
{
	struct proc *p = curproc;

	kn->kn_ptr.p_proc = p;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	knlist_add(&p->p_klist, kn, 0);

	return (0);
}

static void
filt_sigdetach(struct knote *kn)
{
	struct proc *p = kn->kn_ptr.p_proc;

	knlist_remove(&p->p_klist, kn, 0);
}

/*
 * signal knotes are shared with proc knotes, so we apply a mask to 
 * the hint in order to differentiate them from process hints.  This
 * could be avoided by using a signal-specific knote list, but probably
 * isn't worth the trouble.
 */
static int
filt_signal(struct knote *kn, long hint)
{

	if (hint & NOTE_SIGNAL) {
		hint &= ~NOTE_SIGNAL;

		if (kn->kn_id == hint)
			kn->kn_data++;
	}
	return (kn->kn_data != 0);
}

struct sigacts *
sigacts_alloc(void)
{
	struct sigacts *ps;

	ps = malloc(sizeof(struct sigacts), M_SUBPROC, M_WAITOK | M_ZERO);
	ps->ps_refcnt = 1;
	mtx_init(&ps->ps_mtx, "sigacts", NULL, MTX_DEF);
	return (ps);
}

void
sigacts_free(struct sigacts *ps)
{

	mtx_lock(&ps->ps_mtx);
	ps->ps_refcnt--;
	if (ps->ps_refcnt == 0) {
		mtx_destroy(&ps->ps_mtx);
		free(ps, M_SUBPROC);
	} else
		mtx_unlock(&ps->ps_mtx);
}

struct sigacts *
sigacts_hold(struct sigacts *ps)
{
	mtx_lock(&ps->ps_mtx);
	ps->ps_refcnt++;
	mtx_unlock(&ps->ps_mtx);
	return (ps);
}

void
sigacts_copy(struct sigacts *dest, struct sigacts *src)
{

	KASSERT(dest->ps_refcnt == 1, ("sigacts_copy to shared dest"));
	mtx_lock(&src->ps_mtx);
	bcopy(src, dest, offsetof(struct sigacts, ps_refcnt));
	mtx_unlock(&src->ps_mtx);
}

int
sigacts_shared(struct sigacts *ps)
{
	int shared;

	mtx_lock(&ps->ps_mtx);
	shared = ps->ps_refcnt > 1;
	mtx_unlock(&ps->ps_mtx);
	return (shared);
}
