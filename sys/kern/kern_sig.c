/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include "opt_capsicum.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/capsicum.h>
#include <sys/compressor.h>
#include <sys/condvar.h>
#include <sys/devctl.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/ptrace.h>
#include <sys/posix4.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sdt.h>
#include <sys/sbuf.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/timers.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>
#include <sys/wait.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <machine/cpu.h>

#include <security/audit/audit.h>

#define	ONSIG	32		/* NSIG for osig* syscalls.  XXX. */

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE3(proc, , , signal__send,
    "struct thread *", "struct proc *", "int");
SDT_PROBE_DEFINE2(proc, , , signal__clear,
    "int", "ksiginfo_t *");
SDT_PROBE_DEFINE3(proc, , , signal__discard,
    "struct thread *", "struct proc *", "int");

static int	coredump(struct thread *);
static int	killpg1(struct thread *td, int sig, int pgid, int all,
		    ksiginfo_t *ksi);
static int	issignal(struct thread *td);
static void	reschedule_signals(struct proc *p, sigset_t block, int flags);
static int	sigprop(int sig);
static void	tdsigwakeup(struct thread *, int, sig_t, int);
static bool	sig_suspend_threads(struct thread *, struct proc *);
static int	filt_sigattach(struct knote *kn);
static void	filt_sigdetach(struct knote *kn);
static int	filt_signal(struct knote *kn, long hint);
static struct thread *sigtd(struct proc *p, int sig, bool fast_sigblock);
static void	sigqueue_start(void);
static void	sigfastblock_setpend(struct thread *td, bool resched);
static void	sig_handle_first_stop(struct thread *td, struct proc *p,
    int sig);

static uma_zone_t	ksiginfo_zone = NULL;
const struct filterops sig_filtops = {
	.f_isfd = 0,
	.f_attach = filt_sigattach,
	.f_detach = filt_sigdetach,
	.f_event = filt_signal,
};

static int	kern_logsigexit = 1;
SYSCTL_INT(_kern, KERN_LOGSIGEXIT, logsigexit, CTLFLAG_RW,
    &kern_logsigexit, 0,
    "Log processes quitting on abnormal signals to syslog(3)");

static int	kern_forcesigexit = 1;
SYSCTL_INT(_kern, OID_AUTO, forcesigexit, CTLFLAG_RW,
    &kern_forcesigexit, 0, "Force trap signal to be handled");

static SYSCTL_NODE(_kern, OID_AUTO, sigqueue, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "POSIX real time signal");

static int	max_pending_per_proc = 128;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, max_pending_per_proc, CTLFLAG_RW,
    &max_pending_per_proc, 0, "Max pending signals per proc");

static int	preallocate_siginfo = 1024;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, preallocate, CTLFLAG_RDTUN,
    &preallocate_siginfo, 0, "Preallocated signal memory size");

static int	signal_overflow = 0;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, overflow, CTLFLAG_RD,
    &signal_overflow, 0, "Number of signals overflew");

static int	signal_alloc_fail = 0;
SYSCTL_INT(_kern_sigqueue, OID_AUTO, alloc_fail, CTLFLAG_RD,
    &signal_alloc_fail, 0, "signals failed to be allocated");

static int	kern_lognosys = 0;
SYSCTL_INT(_kern, OID_AUTO, lognosys, CTLFLAG_RWTUN, &kern_lognosys, 0,
    "Log invalid syscalls");

static int	kern_signosys = 1;
SYSCTL_INT(_kern, OID_AUTO, signosys, CTLFLAG_RWTUN, &kern_signosys, 0,
    "Send SIGSYS on return from invalid syscall");

__read_frequently bool sigfastblock_fetch_always = false;
SYSCTL_BOOL(_kern, OID_AUTO, sigfastblock_fetch_always, CTLFLAG_RWTUN,
    &sigfastblock_fetch_always, 0,
    "Fetch sigfastblock word on each syscall entry for proper "
    "blocking semantic");

static bool	kern_sig_discard_ign = true;
SYSCTL_BOOL(_kern, OID_AUTO, sig_discard_ign, CTLFLAG_RWTUN,
    &kern_sig_discard_ign, 0,
    "Discard ignored signals on delivery, otherwise queue them to "
    "the target queue");

static bool pt_attach_transparent = true;
SYSCTL_BOOL(_debug, OID_AUTO, ptrace_attach_transparent, CTLFLAG_RWTUN,
    &pt_attach_transparent, 0,
    "Hide wakes from PT_ATTACH on interruptible sleeps");

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

static int	sugid_coredump;
SYSCTL_INT(_kern, OID_AUTO, sugid_coredump, CTLFLAG_RWTUN,
    &sugid_coredump, 0, "Allow setuid and setgid processes to dump core");

static int	capmode_coredump;
SYSCTL_INT(_kern, OID_AUTO, capmode_coredump, CTLFLAG_RWTUN,
    &capmode_coredump, 0, "Allow processes in capability mode to dump core");

static int	do_coredump = 1;
SYSCTL_INT(_kern, OID_AUTO, coredump, CTLFLAG_RW,
	&do_coredump, 0, "Enable/Disable coredumps");

static int	set_core_nodump_flag = 0;
SYSCTL_INT(_kern, OID_AUTO, nodump_coredump, CTLFLAG_RW, &set_core_nodump_flag,
	0, "Enable setting the NODUMP flag on coredump files");

static int	coredump_devctl = 0;
SYSCTL_INT(_kern, OID_AUTO, coredump_devctl, CTLFLAG_RW, &coredump_devctl,
	0, "Generate a devctl notification when processes coredump");

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SIGPROP_KILL		0x01	/* terminates process by default */
#define	SIGPROP_CORE		0x02	/* ditto and coredumps */
#define	SIGPROP_STOP		0x04	/* suspend process */
#define	SIGPROP_TTYSTOP		0x08	/* ditto, from tty */
#define	SIGPROP_IGNORE		0x10	/* ignore by default */
#define	SIGPROP_CONT		0x20	/* continue if suspended */

static const int sigproptbl[NSIG] = {
	[SIGHUP] =	SIGPROP_KILL,
	[SIGINT] =	SIGPROP_KILL,
	[SIGQUIT] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGILL] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGTRAP] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGABRT] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGEMT] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGFPE] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGKILL] =	SIGPROP_KILL,
	[SIGBUS] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGSEGV] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGSYS] =	SIGPROP_KILL | SIGPROP_CORE,
	[SIGPIPE] =	SIGPROP_KILL,
	[SIGALRM] =	SIGPROP_KILL,
	[SIGTERM] =	SIGPROP_KILL,
	[SIGURG] =	SIGPROP_IGNORE,
	[SIGSTOP] =	SIGPROP_STOP,
	[SIGTSTP] =	SIGPROP_STOP | SIGPROP_TTYSTOP,
	[SIGCONT] =	SIGPROP_IGNORE | SIGPROP_CONT,
	[SIGCHLD] =	SIGPROP_IGNORE,
	[SIGTTIN] =	SIGPROP_STOP | SIGPROP_TTYSTOP,
	[SIGTTOU] =	SIGPROP_STOP | SIGPROP_TTYSTOP,
	[SIGIO] =	SIGPROP_IGNORE,
	[SIGXCPU] =	SIGPROP_KILL,
	[SIGXFSZ] =	SIGPROP_KILL,
	[SIGVTALRM] =	SIGPROP_KILL,
	[SIGPROF] =	SIGPROP_KILL,
	[SIGWINCH] =	SIGPROP_IGNORE,
	[SIGINFO] =	SIGPROP_IGNORE,
	[SIGUSR1] =	SIGPROP_KILL,
	[SIGUSR2] =	SIGPROP_KILL,
};

#define	_SIG_FOREACH_ADVANCE(i, set) ({					\
	int __found;							\
	for (;;) {							\
		if (__bits != 0) {					\
			int __sig = ffs(__bits);			\
			__bits &= ~(1u << (__sig - 1));			\
			sig = __i * sizeof((set)->__bits[0]) * NBBY + __sig; \
			__found = 1;					\
			break;						\
		}							\
		if (++__i == _SIG_WORDS) {				\
			__found = 0;					\
			break;						\
		}							\
		__bits = (set)->__bits[__i];				\
	}								\
	__found != 0;							\
})

#define	SIG_FOREACH(i, set)						\
	for (int32_t __i = -1, __bits = 0;				\
	    _SIG_FOREACH_ADVANCE(i, set); )				\

static sigset_t fastblock_mask;

static void
ast_sig(struct thread *td, int tda)
{
	struct proc *p;
	int old_boundary, sig;
	bool resched_sigs;

	p = td->td_proc;

#ifdef DIAGNOSTIC
	if (p->p_numthreads == 1 && (tda & (TDAI(TDA_SIG) |
	    TDAI(TDA_AST))) == 0) {
		PROC_LOCK(p);
		thread_lock(td);
		/*
		 * Note that TDA_SIG should be re-read from
		 * td_ast, since signal might have been delivered
		 * after we cleared td_flags above.  This is one of
		 * the reason for looping check for AST condition.
		 * See comment in userret() about P_PPWAIT.
		 */
		if ((p->p_flag & P_PPWAIT) == 0 &&
		    (td->td_pflags & TDP_SIGFASTBLOCK) == 0) {
			if (SIGPENDING(td) && ((tda | td->td_ast) &
			    (TDAI(TDA_SIG) | TDAI(TDA_AST))) == 0) {
				thread_unlock(td); /* fix dumps */
				panic(
				    "failed2 to set signal flags for ast p %p "
				    "td %p tda %#x td_ast %#x fl %#x",
				    p, td, tda, td->td_ast, td->td_flags);
			}
		}
		thread_unlock(td);
		PROC_UNLOCK(p);
	}
#endif

	/*
	 * Check for signals. Unlocked reads of p_pendingcnt or
	 * p_siglist might cause process-directed signal to be handled
	 * later.
	 */
	if ((tda & TDAI(TDA_SIG)) != 0 || p->p_pendingcnt > 0 ||
	    !SIGISEMPTY(p->p_siglist)) {
		sigfastblock_fetch(td);
		PROC_LOCK(p);
		old_boundary = ~TDB_BOUNDARY | (td->td_dbgflags & TDB_BOUNDARY);
		td->td_dbgflags |= TDB_BOUNDARY;
		mtx_lock(&p->p_sigacts->ps_mtx);
		while ((sig = cursig(td)) != 0) {
			KASSERT(sig >= 0, ("sig %d", sig));
			postsig(sig);
		}
		mtx_unlock(&p->p_sigacts->ps_mtx);
		td->td_dbgflags &= old_boundary;
		PROC_UNLOCK(p);
		resched_sigs = true;
	} else {
		resched_sigs = false;
	}

	/*
	 * Handle deferred update of the fast sigblock value, after
	 * the postsig() loop was performed.
	 */
	sigfastblock_setpend(td, resched_sigs);

	/*
	 * Clear td_sa.code: signal to ptrace that syscall arguments
	 * are unavailable after this point. This AST handler is the
	 * last chance for ptracestop() to signal the tracer before
	 * the tracee returns to userspace.
	 */
	td->td_sa.code = 0;
}

static void
ast_sigsuspend(struct thread *td, int tda __unused)
{
	MPASS((td->td_pflags & TDP_OLDMASK) != 0);
	td->td_pflags &= ~TDP_OLDMASK;
	kern_sigprocmask(td, SIG_SETMASK, &td->td_oldsigmask, NULL, 0);
}

static void
sigqueue_start(void)
{
	ksiginfo_zone = uma_zcreate("ksiginfo", sizeof(ksiginfo_t),
		NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_prealloc(ksiginfo_zone, preallocate_siginfo);
	p31b_setcfg(CTL_P1003_1B_REALTIME_SIGNALS, _POSIX_REALTIME_SIGNALS);
	p31b_setcfg(CTL_P1003_1B_RTSIG_MAX, SIGRTMAX - SIGRTMIN + 1);
	p31b_setcfg(CTL_P1003_1B_SIGQUEUE_MAX, max_pending_per_proc);
	SIGFILLSET(fastblock_mask);
	SIG_CANTMASK(fastblock_mask);
	ast_register(TDA_SIG, ASTR_UNCOND, 0, ast_sig);

	/*
	 * TDA_PSELECT is for the case where the signal mask should be restored
	 * before delivering any signals so that we do not deliver any that are
	 * blocked by the normal thread mask.  It is mutually exclusive with
	 * TDA_SIGSUSPEND, which should be used if we *do* want to deliver
	 * signals that are normally blocked, e.g., if it interrupted our sleep.
	 */
	ast_register(TDA_PSELECT, ASTR_ASTF_REQUIRED | ASTR_TDP,
	    TDP_OLDMASK, ast_sigsuspend);
	ast_register(TDA_SIGSUSPEND, ASTR_ASTF_REQUIRED | ASTR_TDP,
	    TDP_OLDMASK, ast_sigsuspend);
}

ksiginfo_t *
ksiginfo_alloc(int mwait)
{
	MPASS(mwait == M_WAITOK || mwait == M_NOWAIT);

	if (ksiginfo_zone == NULL)
		return (NULL);
	return (uma_zalloc(ksiginfo_zone, mwait | M_ZERO));
}

void
ksiginfo_free(ksiginfo_t *ksi)
{
	uma_zfree(ksiginfo_zone, ksi);
}

static __inline bool
ksiginfo_tryfree(ksiginfo_t *ksi)
{
	if ((ksi->ksi_flags & KSI_EXT) == 0) {
		uma_zfree(ksiginfo_zone, ksi);
		return (true);
	}
	return (false);
}

void
sigqueue_init(sigqueue_t *list, struct proc *p)
{
	SIGEMPTYSET(list->sq_signals);
	SIGEMPTYSET(list->sq_kill);
	SIGEMPTYSET(list->sq_ptrace);
	TAILQ_INIT(&list->sq_list);
	list->sq_proc = p;
	list->sq_flags = SQ_INIT;
}

/*
 * Get a signal's ksiginfo.
 * Return:
 *	0	-	signal not found
 *	others	-	signal number
 */
static int
sigqueue_get(sigqueue_t *sq, int signo, ksiginfo_t *si)
{
	struct proc *p = sq->sq_proc;
	struct ksiginfo *ksi, *next;
	int count = 0;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));

	if (!SIGISMEMBER(sq->sq_signals, signo))
		return (0);

	if (SIGISMEMBER(sq->sq_ptrace, signo)) {
		count++;
		SIGDELSET(sq->sq_ptrace, signo);
		si->ksi_flags |= KSI_PTRACE;
	}
	if (SIGISMEMBER(sq->sq_kill, signo)) {
		count++;
		if (count == 1)
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
	if (kp == NULL && !SIGISMEMBER(sq->sq_kill, ksi->ksi_signo) &&
	    !SIGISMEMBER(sq->sq_ptrace, ksi->ksi_signo))
		SIGDELSET(sq->sq_signals, ksi->ksi_signo);
}

static int
sigqueue_add(sigqueue_t *sq, int signo, ksiginfo_t *si)
{
	struct proc *p = sq->sq_proc;
	struct ksiginfo *ksi;
	int ret = 0;

	KASSERT(sq->sq_flags & SQ_INIT, ("sigqueue not inited"));

	/*
	 * SIGKILL/SIGSTOP cannot be caught or masked, so take the fast path
	 * for these signals.
	 */
	if (signo == SIGKILL || signo == SIGSTOP || si == NULL) {
		SIGADDSET(sq->sq_kill, signo);
		goto out_set_bit;
	}

	/* directly insert the ksi, don't copy it */
	if (si->ksi_flags & KSI_INS) {
		if (si->ksi_flags & KSI_HEAD)
			TAILQ_INSERT_HEAD(&sq->sq_list, si, ksi_link);
		else
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
	} else if ((ksi = ksiginfo_alloc(M_NOWAIT)) == NULL) {
		signal_alloc_fail++;
		ret = EAGAIN;
	} else {
		if (p != NULL)
			p->p_pendingcnt++;
		ksiginfo_copy(si, ksi);
		ksi->ksi_signo = signo;
		if (si->ksi_flags & KSI_HEAD)
			TAILQ_INSERT_HEAD(&sq->sq_list, ksi, ksi_link);
		else
			TAILQ_INSERT_TAIL(&sq->sq_list, ksi, ksi_link);
		ksi->ksi_sigq = sq;
	}

	if (ret != 0) {
		if ((si->ksi_flags & KSI_PTRACE) != 0) {
			SIGADDSET(sq->sq_ptrace, signo);
			ret = 0;
			goto out_set_bit;
		} else if ((si->ksi_flags & KSI_TRAP) != 0 ||
		    (si->ksi_flags & KSI_SIGQ) == 0) {
			SIGADDSET(sq->sq_kill, signo);
			ret = 0;
			goto out_set_bit;
		}
		return (ret);
	}

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
	SIGEMPTYSET(sq->sq_ptrace);
}

static void
sigqueue_move_set(sigqueue_t *src, sigqueue_t *dst, const sigset_t *set)
{
	sigset_t tmp;
	struct proc *p1, *p2;
	ksiginfo_t *ksi, *next;

	KASSERT(src->sq_flags & SQ_INIT, ("src sigqueue not inited"));
	KASSERT(dst->sq_flags & SQ_INIT, ("dst sigqueue not inited"));
	p1 = src->sq_proc;
	p2 = dst->sq_proc;
	/* Move siginfo to target list */
	TAILQ_FOREACH_SAFE(ksi, &src->sq_list, ksi_link, next) {
		if (SIGISMEMBER(*set, ksi->ksi_signo)) {
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
	SIGSETAND(tmp, *set);
	SIGSETOR(dst->sq_kill, tmp);
	SIGSETNAND(src->sq_kill, tmp);

	tmp = src->sq_ptrace;
	SIGSETAND(tmp, *set);
	SIGSETOR(dst->sq_ptrace, tmp);
	SIGSETNAND(src->sq_ptrace, tmp);

	tmp = src->sq_signals;
	SIGSETAND(tmp, *set);
	SIGSETOR(dst->sq_signals, tmp);
	SIGSETNAND(src->sq_signals, tmp);
}

#if 0
static void
sigqueue_move(sigqueue_t *src, sigqueue_t *dst, int signo)
{
	sigset_t set;

	SIGEMPTYSET(set);
	SIGADDSET(set, signo);
	sigqueue_move_set(src, dst, &set);
}
#endif

static void
sigqueue_delete_set(sigqueue_t *sq, const sigset_t *set)
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
	SIGSETNAND(sq->sq_ptrace, *set);
	SIGSETNAND(sq->sq_signals, *set);
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
static void
sigqueue_delete_set_proc(struct proc *p, const sigset_t *set)
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

static void
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
 * Determine signal that should be delivered to thread td, the current
 * thread, 0 if none.  If there is a pending stop signal with default
 * action, the process stops in issignal().
 */
int
cursig(struct thread *td)
{
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	mtx_assert(&td->td_proc->p_sigacts->ps_mtx, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_NOTOWNED);
	return (SIGPENDING(td) ? issignal(td) : 0);
}

/*
 * Arrange for ast() to handle unmasked pending signals on return to user
 * mode.  This must be called whenever a signal is added to td_sigqueue or
 * unmasked in td_sigmask.
 */
void
signotify(struct thread *td)
{

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);

	if (SIGPENDING(td))
		ast_sched(td, TDA_SIG);
}

/*
 * Returns 1 (true) if altstack is configured for the thread, and the
 * passed stack bottom address falls into the altstack range.  Handles
 * the 43 compat special case where the alt stack size is zero.
 */
int
sigonstack(size_t sp)
{
	struct thread *td;

	td = curthread;
	if ((td->td_pflags & TDP_ALTSTACK) == 0)
		return (0);
#if defined(COMPAT_43)
	if (SV_PROC_FLAG(td->td_proc, SV_AOUT) && td->td_sigstk.ss_size == 0)
		return ((td->td_sigstk.ss_flags & SS_ONSTACK) != 0);
#endif
	return (sp >= (size_t)td->td_sigstk.ss_sp &&
	    sp < td->td_sigstk.ss_size + (size_t)td->td_sigstk.ss_sp);
}

static __inline int
sigprop(int sig)
{

	if (sig > 0 && sig < nitems(sigproptbl))
		return (sigproptbl[sig]);
	return (0);
}

static bool
sigact_flag_test(const struct sigaction *act, int flag)
{

	/*
	 * SA_SIGINFO is reset when signal disposition is set to
	 * ignore or default.  Other flags are kept according to user
	 * settings.
	 */
	return ((act->sa_flags & flag) != 0 && (flag != SA_SIGINFO ||
	    ((__sighandler_t *)act->sa_sigaction != SIG_IGN &&
	    (__sighandler_t *)act->sa_sigaction != SIG_DFL)));
}

/*
 * kern_sigaction
 * sigaction
 * freebsd4_sigaction
 * osigaction
 */
int
kern_sigaction(struct thread *td, int sig, const struct sigaction *act,
    struct sigaction *oact, int flags)
{
	struct sigacts *ps;
	struct proc *p = td->td_proc;

	if (!_SIG_VALID(sig))
		return (EINVAL);
	if (act != NULL && act->sa_handler != SIG_DFL &&
	    act->sa_handler != SIG_IGN && (act->sa_flags & ~(SA_ONSTACK |
	    SA_RESTART | SA_RESETHAND | SA_NOCLDSTOP | SA_NODEFER |
	    SA_NOCLDWAIT | SA_SIGINFO)) != 0)
		return (EINVAL);

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if (oact) {
		memset(oact, 0, sizeof(*oact));
		oact->sa_mask = ps->ps_catchmask[_SIG_IDX(sig)];
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
		if (sigact_flag_test(act, SA_SIGINFO)) {
			ps->ps_sigact[_SIG_IDX(sig)] =
			    (__sighandler_t *)act->sa_sigaction;
			SIGADDSET(ps->ps_siginfo, sig);
		} else {
			ps->ps_sigact[_SIG_IDX(sig)] = act->sa_handler;
			SIGDELSET(ps->ps_siginfo, sig);
		}
		if (!sigact_flag_test(act, SA_RESTART))
			SIGADDSET(ps->ps_sigintr, sig);
		else
			SIGDELSET(ps->ps_sigintr, sig);
		if (sigact_flag_test(act, SA_ONSTACK))
			SIGADDSET(ps->ps_sigonstack, sig);
		else
			SIGDELSET(ps->ps_sigonstack, sig);
		if (sigact_flag_test(act, SA_RESETHAND))
			SIGADDSET(ps->ps_sigreset, sig);
		else
			SIGDELSET(ps->ps_sigreset, sig);
		if (sigact_flag_test(act, SA_NODEFER))
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
		    (sigprop(sig) & SIGPROP_IGNORE &&
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
sys_sigaction(struct thread *td, struct sigaction_args *uap)
{
	struct sigaction act, oact;
	struct sigaction *actp, *oactp;
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
freebsd4_sigaction(struct thread *td, struct freebsd4_sigaction_args *uap)
{
	struct sigaction act, oact;
	struct sigaction *actp, *oactp;
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
osigaction(struct thread *td, struct osigaction_args *uap)
{
	struct osigaction sa;
	struct sigaction nsa, osa;
	struct sigaction *nsap, *osap;
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
osigreturn(struct thread *td, struct osigreturn_args *uap)
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
siginit(struct proc *p)
{
	int i;
	struct sigacts *ps;

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	for (i = 1; i <= NSIG; i++) {
		if (sigprop(i) & SIGPROP_IGNORE && i != SIGCONT) {
			SIGADDSET(ps->ps_sigignore, i);
		}
	}
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
}

/*
 * Reset specified signal to the default disposition.
 */
static void
sigdflt(struct sigacts *ps, int sig)
{

	mtx_assert(&ps->ps_mtx, MA_OWNED);
	SIGDELSET(ps->ps_sigcatch, sig);
	if ((sigprop(sig) & SIGPROP_IGNORE) != 0 && sig != SIGCONT)
		SIGADDSET(ps->ps_sigignore, sig);
	ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
	SIGDELSET(ps->ps_siginfo, sig);
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps;
	struct thread *td;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through td_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	sig_drop_caught(p);

	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	td = curthread;
	MPASS(td->td_proc == p);
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
	if ((flags & SIGPROCMASK_PROC_LOCKED) != 0)
		PROC_LOCK_ASSERT(p, MA_OWNED);
	else
		PROC_LOCK(p);
	mtx_assert(&p->p_sigacts->ps_mtx, (flags & SIGPROCMASK_PS_LOCKED) != 0
	    ? MA_OWNED : MA_NOTOWNED);
	if (oset != NULL)
		*oset = td->td_sigmask;

	error = 0;
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
			goto out;
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
			goto out;
		}

		/*
		 * The new_block set contains signals that were not previously
		 * blocked, but are blocked now.
		 *
		 * In case we block any signal that was not previously blocked
		 * for td, and process has the signal pending, try to schedule
		 * signal delivery to some thread that does not block the
		 * signal, possibly waking it up.
		 */
		if (p->p_numthreads != 1)
			reschedule_signals(p, new_block, flags);
	}

out:
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
sys_sigprocmask(struct thread *td, struct sigprocmask_args *uap)
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
osigprocmask(struct thread *td, struct osigprocmask_args *uap)
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
sys_sigwait(struct thread *td, struct sigwait_args *uap)
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
		/*
		 * sigwait() function shall not return EINTR, but
		 * the syscall does.  Non-ancient libc provides the
		 * wrapper which hides EINTR.  Otherwise, EINTR return
		 * is used by libthr to handle required cancellation
		 * point in the sigwait().
		 */
		if (error == EINTR && td->td_proc->p_osrel < P_OSREL_SIGWAIT)
			return (ERESTART);
		td->td_retval[0] = error;
		return (0);
	}

	error = copyout(&ksi.ksi_signo, uap->sig, sizeof(ksi.ksi_signo));
	td->td_retval[0] = error;
	return (0);
}

int
sys_sigtimedwait(struct thread *td, struct sigtimedwait_args *uap)
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
sys_sigwaitinfo(struct thread *td, struct sigwaitinfo_args *uap)
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

static void
proc_td_siginfo_capture(struct thread *td, siginfo_t *si)
{
	struct thread *thr;

	FOREACH_THREAD_IN_PROC(td->td_proc, thr) {
		if (thr == td)
			thr->td_si = *si;
		else
			thr->td_si.si_signo = 0;
	}
}

int
kern_sigtimedwait(struct thread *td, sigset_t waitset, ksiginfo_t *ksi,
	struct timespec *timeout)
{
	struct sigacts *ps;
	sigset_t saved_mask, new_block;
	struct proc *p;
	int error, sig, timevalid = 0;
	sbintime_t sbt, precision, tsbt;
	struct timespec ts;
	bool traced;

	p = td->td_proc;
	error = 0;
	traced = false;

	/* Ensure the sigfastblock value is up to date. */
	sigfastblock_fetch(td);

	if (timeout != NULL) {
		if (timeout->tv_nsec >= 0 && timeout->tv_nsec < 1000000000) {
			timevalid = 1;
			ts = *timeout;
			if (ts.tv_sec < INT32_MAX / 2) {
				tsbt = tstosbt(ts);
				precision = tsbt;
				precision >>= tc_precexp;
				if (TIMESEL(&sbt, tsbt))
					sbt += tc_tick_sbt;
				sbt += tsbt;
			} else
				precision = sbt = 0;
		}
	} else
		precision = sbt = 0;
	ksiginfo_init(ksi);
	/* Some signals can not be waited for. */
	SIG_CANTMASK(waitset);
	ps = p->p_sigacts;
	PROC_LOCK(p);
	saved_mask = td->td_sigmask;
	SIGSETNAND(td->td_sigmask, waitset);
	if ((p->p_sysent->sv_flags & SV_SIG_DISCIGN) != 0 ||
	    !kern_sig_discard_ign) {
		thread_lock(td);
		td->td_flags |= TDF_SIGWAIT;
		thread_unlock(td);
	}
	for (;;) {
		mtx_lock(&ps->ps_mtx);
		sig = cursig(td);
		mtx_unlock(&ps->ps_mtx);
		KASSERT(sig >= 0, ("sig %d", sig));
		if (sig != 0 && SIGISMEMBER(waitset, sig)) {
			if (sigqueue_get(&td->td_sigqueue, sig, ksi) != 0 ||
			    sigqueue_get(&p->p_sigqueue, sig, ksi) != 0) {
				error = 0;
				break;
			}
		}

		if (error != 0)
			break;

		/*
		 * POSIX says this must be checked after looking for pending
		 * signals.
		 */
		if (timeout != NULL && !timevalid) {
			error = EINVAL;
			break;
		}

		if (traced) {
			error = EINTR;
			break;
		}

		error = msleep_sbt(&p->p_sigacts, &p->p_mtx, PPAUSE | PCATCH,
		    "sigwait", sbt, precision, C_ABSOLUTE);

		/* The syscalls can not be restarted. */
		if (error == ERESTART)
			error = EINTR;

		/*
		 * If PTRACE_SCE or PTRACE_SCX were set after
		 * userspace entered the syscall, return spurious
		 * EINTR after wait was done.  Only do this as last
		 * resort after rechecking for possible queued signals
		 * and expired timeouts.
		 */
		if (error == 0 && (p->p_ptevents & PTRACE_SYSCALL) != 0)
			traced = true;
	}
	thread_lock(td);
	td->td_flags &= ~TDF_SIGWAIT;
	thread_unlock(td);

	new_block = saved_mask;
	SIGSETNAND(new_block, td->td_sigmask);
	td->td_sigmask = saved_mask;
	/*
	 * Fewer signals can be delivered to us, reschedule signal
	 * notification.
	 */
	if (p->p_numthreads != 1)
		reschedule_signals(p, new_block, 0);

	if (error == 0) {
		SDT_PROBE2(proc, , , signal__clear, sig, ksi);

		if (ksi->ksi_code == SI_TIMER)
			itimer_accept(p, ksi->ksi_timerid, ksi);

#ifdef KTRACE
		if (KTRPOINT(td, KTR_PSIG)) {
			sig_t action;

			mtx_lock(&ps->ps_mtx);
			action = ps->ps_sigact[_SIG_IDX(sig)];
			mtx_unlock(&ps->ps_mtx);
			ktrpsig(sig, action, &td->td_sigmask, ksi->ksi_code);
		}
#endif
		if (sig == SIGKILL) {
			proc_td_siginfo_capture(td, &ksi->ksi_info);
			sigexit(td, sig);
		}
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
sys_sigpending(struct thread *td, struct sigpending_args *uap)
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
osigpending(struct thread *td, struct osigpending_args *uap)
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
osigvec(struct thread *td, struct osigvec_args *uap)
{
	struct sigvec vec;
	struct sigaction nsa, osa;
	struct sigaction *nsap, *osap;
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
osigblock(struct thread *td, struct osigblock_args *uap)
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
osigsetmask(struct thread *td, struct osigsetmask_args *uap)
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
sys_sigsuspend(struct thread *td, struct sigsuspend_args *uap)
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

	/* Ensure the sigfastblock value is up to date. */
	sigfastblock_fetch(td);

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
	ast_sched(td, TDA_SIGSUSPEND);

	/*
	 * Process signals now. Otherwise, we can get spurious wakeup
	 * due to signal entered process queue, but delivered to other
	 * thread. But sigsuspend should return only on signal
	 * delivery.
	 */
	(p->p_sysent->sv_set_syscall_retval)(td, EINTR);
	for (has_sig = 0; !has_sig;) {
		while (msleep(&p->p_sigacts, &p->p_mtx, PPAUSE|PCATCH, "pause",
			0) == 0)
			/* void */;
		thread_suspend_check(0);
		mtx_lock(&p->p_sigacts->ps_mtx);
		while ((sig = cursig(td)) != 0) {
			KASSERT(sig >= 0, ("sig %d", sig));
			has_sig += postsig(sig);
		}
		mtx_unlock(&p->p_sigacts->ps_mtx);

		/*
		 * If PTRACE_SCE or PTRACE_SCX were set after
		 * userspace entered the syscall, return spurious
		 * EINTR.
		 */
		if ((p->p_ptevents & PTRACE_SYSCALL) != 0)
			has_sig += 1;
	}
	PROC_UNLOCK(p);
	td->td_errno = EINTR;
	td->td_pflags |= TDP_NERRNO;
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
osigsuspend(struct thread *td, struct osigsuspend_args *uap)
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
osigstack(struct thread *td, struct osigstack_args *uap)
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
sys_sigaltstack(struct thread *td, struct sigaltstack_args *uap)
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

struct killpg1_ctx {
	struct thread *td;
	ksiginfo_t *ksi;
	int sig;
	bool sent;
	bool found;
	int ret;
};

static void
killpg1_sendsig_locked(struct proc *p, struct killpg1_ctx *arg)
{
	int err;

	err = p_cansignal(arg->td, p, arg->sig);
	if (err == 0 && arg->sig != 0)
		pksignal(p, arg->sig, arg->ksi);
	if (err != ESRCH)
		arg->found = true;
	if (err == 0)
		arg->sent = true;
	else if (arg->ret == 0 && err != ESRCH && err != EPERM)
		arg->ret = err;
}

static void
killpg1_sendsig(struct proc *p, bool notself, struct killpg1_ctx *arg)
{

	if (p->p_pid <= 1 || (p->p_flag & P_SYSTEM) != 0 ||
	    (notself && p == arg->td->td_proc) || p->p_state == PRS_NEW)
		return;

	PROC_LOCK(p);
	killpg1_sendsig_locked(p, arg);
	PROC_UNLOCK(p);
}

static void
kill_processes_prison_cb(struct proc *p, void *arg)
{
	struct killpg1_ctx *ctx = arg;

	if (p->p_pid <= 1 || (p->p_flag & P_SYSTEM) != 0 ||
	    (p == ctx->td->td_proc) || p->p_state == PRS_NEW)
		return;

	killpg1_sendsig_locked(p, ctx);
}

/*
 * Common code for kill process group/broadcast kill.
 * td is the calling thread, as usual.
 */
static int
killpg1(struct thread *td, int sig, int pgid, int all, ksiginfo_t *ksi)
{
	struct proc *p;
	struct pgrp *pgrp;
	struct killpg1_ctx arg;

	arg.td = td;
	arg.ksi = ksi;
	arg.sig = sig;
	arg.sent = false;
	arg.found = false;
	arg.ret = 0;
	if (all) {
		/*
		 * broadcast
		 */
		prison_proc_iterate(td->td_ucred->cr_prison,
		    kill_processes_prison_cb, &arg);
	} else {
again:
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
		if (!sx_try_xlock(&pgrp->pg_killsx)) {
			PGRP_UNLOCK(pgrp);
			sx_xlock(&pgrp->pg_killsx);
			sx_xunlock(&pgrp->pg_killsx);
			goto again;
		}
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			killpg1_sendsig(p, false, &arg);
		}
		PGRP_UNLOCK(pgrp);
		sx_xunlock(&pgrp->pg_killsx);
	}
	MPASS(arg.ret != 0 || arg.found || !arg.sent);
	if (arg.ret == 0 && !arg.sent)
		arg.ret = arg.found ? EPERM : ESRCH;
	return (arg.ret);
}

#ifndef _SYS_SYSPROTO_H_
struct kill_args {
	int	pid;
	int	signum;
};
#endif
/* ARGSUSED */
int
sys_kill(struct thread *td, struct kill_args *uap)
{

	return (kern_kill(td, uap->pid, uap->signum));
}

int
kern_kill(struct thread *td, pid_t pid, int signum)
{
	ksiginfo_t ksi;
	struct proc *p;
	int error;

	/*
	 * A process in capability mode can send signals only to himself.
	 * The main rationale behind this is that abort(3) is implemented as
	 * kill(getpid(), SIGABRT).
	 */
	if (pid != td->td_proc->p_pid) {
		if (CAP_TRACING(td))
			ktrcapfail(CAPFAIL_SIGNAL, &signum);
		if (IN_CAPABILITY_MODE(td))
			return (ECAPMODE);
	}

	AUDIT_ARG_SIGNUM(signum);
	AUDIT_ARG_PID(pid);
	if ((u_int)signum > _SIG_MAXSIG)
		return (EINVAL);

	ksiginfo_init(&ksi);
	ksi.ksi_signo = signum;
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_ucred->cr_ruid;

	if (pid > 0) {
		/* kill single process */
		if ((p = pfind_any(pid)) == NULL)
			return (ESRCH);
		AUDIT_ARG_PROCESS(p);
		error = p_cansignal(td, p, signum);
		if (error == 0 && signum)
			pksignal(p, signum, &ksi);
		PROC_UNLOCK(p);
		return (error);
	}
	switch (pid) {
	case -1:		/* broadcast signal */
		return (killpg1(td, signum, 0, 1, &ksi));
	case 0:			/* signal own process group */
		return (killpg1(td, signum, 0, 0, &ksi));
	default:		/* negative explicit process group */
		return (killpg1(td, signum, -pid, 0, &ksi));
	}
	/* NOTREACHED */
}

int
sys_pdkill(struct thread *td, struct pdkill_args *uap)
{
	struct proc *p;
	int error;

	AUDIT_ARG_SIGNUM(uap->signum);
	AUDIT_ARG_FD(uap->fd);
	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	error = procdesc_find(td, uap->fd, &cap_pdkill_rights, &p);
	if (error)
		return (error);
	AUDIT_ARG_PROCESS(p);
	error = p_cansignal(td, p, uap->signum);
	if (error == 0 && uap->signum)
		kern_psignal(p, uap->signum);
	PROC_UNLOCK(p);
	return (error);
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
sys_sigqueue(struct thread *td, struct sigqueue_args *uap)
{
	union sigval sv;

	sv.sival_ptr = uap->value;

	return (kern_sigqueue(td, uap->pid, uap->signum, &sv));
}

int
kern_sigqueue(struct thread *td, pid_t pid, int signumf, union sigval *value)
{
	ksiginfo_t ksi;
	struct proc *p;
	struct thread *td2;
	u_int signum;
	int error;

	signum = signumf & ~__SIGQUEUE_TID;
	if (signum > _SIG_MAXSIG)
		return (EINVAL);

	/*
	 * Specification says sigqueue can only send signal to
	 * single process.
	 */
	if (pid <= 0)
		return (EINVAL);

	if ((signumf & __SIGQUEUE_TID) == 0) {
		if ((p = pfind_any(pid)) == NULL)
			return (ESRCH);
		td2 = NULL;
	} else {
		p = td->td_proc;
		td2 = tdfind((lwpid_t)pid, p->p_pid);
		if (td2 == NULL)
			return (ESRCH);
	}

	error = p_cansignal(td, p, signum);
	if (error == 0 && signum != 0) {
		ksiginfo_init(&ksi);
		ksi.ksi_flags = KSI_SIGQ;
		ksi.ksi_signo = signum;
		ksi.ksi_code = SI_QUEUE;
		ksi.ksi_pid = td->td_proc->p_pid;
		ksi.ksi_uid = td->td_ucred->cr_ruid;
		ksi.ksi_value = *value;
		error = tdsendsignal(p, td2, ksi.ksi_signo, &ksi);
	}
	PROC_UNLOCK(p);
	return (error);
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
			if (p->p_state == PRS_NORMAL &&
			    (checkctty == 0 || p->p_flag & P_CONTROLT))
				pksignal(p, sig, ksi);
			PROC_UNLOCK(p);
		}
	}
}

/*
 * Recalculate the signal mask and reset the signal disposition after
 * usermode frame for delivery is formed.  Should be called after
 * mach-specific routine, because sysent->sv_sendsig() needs correct
 * ps_siginfo and signal mask.
 */
static void
postsig_done(int sig, struct thread *td, struct sigacts *ps)
{
	sigset_t mask;

	mtx_assert(&ps->ps_mtx, MA_OWNED);
	td->td_ru.ru_nsignals++;
	mask = ps->ps_catchmask[_SIG_IDX(sig)];
	if (!SIGISMEMBER(ps->ps_signodefer, sig))
		SIGADDSET(mask, sig);
	kern_sigprocmask(td, SIG_BLOCK, &mask, NULL,
	    SIGPROCMASK_PROC_LOCKED | SIGPROCMASK_PS_LOCKED);
	if (SIGISMEMBER(ps->ps_sigreset, sig))
		sigdflt(ps, sig);
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
	struct proc *p;
	sigset_t sigmask;
	int sig;

	p = td->td_proc;
	sig = ksi->ksi_signo;
	KASSERT(_SIG_VALID(sig), ("invalid signal"));

	sigfastblock_fetch(td);
	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	sigmask = td->td_sigmask;
	if (td->td_sigblock_val != 0)
		SIGSETOR(sigmask, fastblock_mask);
	if ((p->p_flag & P_TRACED) == 0 && SIGISMEMBER(ps->ps_sigcatch, sig) &&
	    !SIGISMEMBER(sigmask, sig)) {
#ifdef KTRACE
		if (KTRPOINT(curthread, KTR_PSIG))
			ktrpsig(sig, ps->ps_sigact[_SIG_IDX(sig)],
			    &td->td_sigmask, ksi->ksi_code);
#endif
		(*p->p_sysent->sv_sendsig)(ps->ps_sigact[_SIG_IDX(sig)],
		    ksi, &td->td_sigmask);
		postsig_done(sig, td, ps);
		mtx_unlock(&ps->ps_mtx);
	} else {
		/*
		 * Avoid a possible infinite loop if the thread
		 * masking the signal or process is ignoring the
		 * signal.
		 */
		if (kern_forcesigexit && (SIGISMEMBER(sigmask, sig) ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN)) {
			SIGDELSET(td->td_sigmask, sig);
			SIGDELSET(ps->ps_sigcatch, sig);
			SIGDELSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
			td->td_pflags &= ~TDP_SIGFASTBLOCK;
			td->td_sigblock_val = 0;
		}
		mtx_unlock(&ps->ps_mtx);
		p->p_sig = sig;		/* XXX to verify code */
		tdsendsignal(p, td, sig, ksi);
	}
	PROC_UNLOCK(p);
}

static struct thread *
sigtd(struct proc *p, int sig, bool fast_sigblock)
{
	struct thread *td, *signal_td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS(!fast_sigblock || p == curproc);

	/*
	 * Check if current thread can handle the signal without
	 * switching context to another thread.
	 */
	if (curproc == p && !SIGISMEMBER(curthread->td_sigmask, sig) &&
	    (!fast_sigblock || curthread->td_sigblock_val == 0))
		return (curthread);

	/* Find a non-stopped thread that does not mask the signal. */
	signal_td = NULL;
	FOREACH_THREAD_IN_PROC(p, td) {
		if (!SIGISMEMBER(td->td_sigmask, sig) && (!fast_sigblock ||
		    td != curthread || td->td_sigblock_val == 0) &&
		    (td->td_flags & TDF_BOUNDARY) == 0) {
			signal_td = td;
			break;
		}
	}
	/* Select random (first) thread if no better match was found. */
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
kern_psignal(struct proc *p, int sig)
{
	ksiginfo_t ksi;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_KERNEL;
	(void) tdsendsignal(p, NULL, sig, &ksi);
}

int
pksignal(struct proc *p, int sig, ksiginfo_t *ksi)
{

	return (tdsendsignal(p, NULL, sig, ksi));
}

/* Utility function for finding a thread to send signal event to. */
int
sigev_findtd(struct proc *p, struct sigevent *sigev, struct thread **ttd)
{
	struct thread *td;

	if (sigev->sigev_notify == SIGEV_THREAD_ID) {
		td = tdfind(sigev->sigev_notify_thread_id, p->p_pid);
		if (td == NULL)
			return (ESRCH);
		*ttd = td;
	} else {
		*ttd = NULL;
		PROC_LOCK(p);
	}
	return (0);
}

void
tdsignal(struct thread *td, int sig)
{
	ksiginfo_t ksi;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_KERNEL;
	(void) tdsendsignal(td->td_proc, td, sig, &ksi);
}

void
tdksignal(struct thread *td, int sig, ksiginfo_t *ksi)
{

	(void) tdsendsignal(td->td_proc, td, sig, ksi);
}

static void
sig_sleepq_abort(struct thread *td, int intrval)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);

	if (intrval == 0 && (td->td_flags & TDF_SIGWAIT) == 0)
		thread_unlock(td);
	else
		sleepq_abort(td, intrval);
}

int
tdsendsignal(struct proc *p, struct thread *td, int sig, ksiginfo_t *ksi)
{
	sig_t action;
	sigqueue_t *sigqueue;
	struct sigacts *ps;
	int intrval, prop, ret;

	MPASS(td == NULL || p == td->td_proc);
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (!_SIG_VALID(sig))
		panic("%s(): invalid signal %d", __func__, sig);

	KASSERT(ksi == NULL || !KSI_ONQ(ksi), ("%s: ksi on queue", __func__));

	/*
	 * IEEE Std 1003.1-2001: return success when killing a zombie.
	 */
	if (p->p_state == PRS_ZOMBIE) {
		if (ksi != NULL && (ksi->ksi_flags & KSI_INS) != 0)
			ksiginfo_tryfree(ksi);
		return (0);
	}

	ps = p->p_sigacts;
	KNOTE_LOCKED(p->p_klist, NOTE_SIGNAL | sig);
	prop = sigprop(sig);

	if (td == NULL) {
		td = sigtd(p, sig, false);
		sigqueue = &p->p_sigqueue;
	} else
		sigqueue = &td->td_sigqueue;

	SDT_PROBE3(proc, , , signal__send, td, p, sig);

	/*
	 * If the signal is being ignored, then we forget about it
	 * immediately, except when the target process executes
	 * sigwait().  (Note: we don't set SIGCONT in ps_sigignore,
	 * and if it is set to SIG_IGN, action will be SIG_DFL here.)
	 */
	mtx_lock(&ps->ps_mtx);
	if (SIGISMEMBER(ps->ps_sigignore, sig)) {
		if (kern_sig_discard_ign &&
		    (p->p_sysent->sv_flags & SV_SIG_DISCIGN) == 0) {
			SDT_PROBE3(proc, , , signal__discard, td, p, sig);

			mtx_unlock(&ps->ps_mtx);
			if (ksi != NULL && (ksi->ksi_flags & KSI_INS) != 0)
				ksiginfo_tryfree(ksi);
			return (0);
		} else {
			action = SIG_CATCH;
			intrval = 0;
		}
	} else {
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
	}
	mtx_unlock(&ps->ps_mtx);

	if (prop & SIGPROP_CONT)
		sigqueue_delete_stopmask_proc(p);
	else if (prop & SIGPROP_STOP) {
		if (pt_attach_transparent &&
		    (p->p_flag & P_TRACED) != 0 &&
		    (p->p_flag2 & P2_PTRACE_FSTP) != 0) {
			PROC_SLOCK(p);
			sig_handle_first_stop(NULL, p, sig);
			PROC_SUNLOCK(p);
			return (0);
		}

		/*
		 * If sending a tty stop signal to a member of an orphaned
		 * process group, discard the signal here if the action
		 * is default; don't stop the process below if sleeping,
		 * and don't clear any pending SIGCONT.
		 */
		if ((prop & SIGPROP_TTYSTOP) != 0 &&
		    (p->p_pgrp->pg_flags & PGRP_ORPHANED) != 0 &&
		    action == SIG_DFL) {
			if (ksi != NULL && (ksi->ksi_flags & KSI_INS) != 0)
				ksiginfo_tryfree(ksi);
			return (0);
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
	    !((prop & SIGPROP_CONT) && (p->p_flag & P_STOPPED_SIG)))
		return (0);

	/*
	 * Some signals have a process-wide effect and a per-thread
	 * component.  Most processing occurs when the process next
	 * tries to cross the user boundary, however there are some
	 * times when processing needs to be done immediately, such as
	 * waking up threads so that they can cross the user boundary.
	 * We try to do the per-process part here.
	 */
	if (P_SHOULDSTOP(p)) {
		KASSERT(!(p->p_flag & P_WEXIT),
		    ("signal to stopped but exiting process"));
		if (sig == SIGKILL) {
			/*
			 * If traced process is already stopped,
			 * then no further action is necessary.
			 */
			if (p->p_flag & P_TRACED)
				return (0);
			/*
			 * SIGKILL sets process running.
			 * It will die elsewhere.
			 * All threads must be restarted.
			 */
			p->p_flag &= ~P_STOPPED_SIG;
			goto runfast;
		}

		if (prop & SIGPROP_CONT) {
			/*
			 * If traced process is already stopped,
			 * then no further action is necessary.
			 */
			if (p->p_flag & P_TRACED)
				return (0);
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
				p->p_xsig = SIGCONT;
				PROC_LOCK(p->p_pptr);
				childproc_continued(p);
				PROC_UNLOCK(p->p_pptr);
				PROC_SLOCK(p);
			}
			if (action == SIG_DFL) {
				thread_unsuspend(p);
				PROC_SUNLOCK(p);
				sigqueue_delete(sigqueue, sig);
				goto out_cont;
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
			goto out_cont;
		}

		if (prop & SIGPROP_STOP) {
			/*
			 * If traced process is already stopped,
			 * then no further action is necessary.
			 */
			if (p->p_flag & P_TRACED)
				return (0);
			/*
			 * Already stopped, don't need to stop again
			 * (If we did the shell could get confused).
			 * Just make sure the signal STOP bit set.
			 */
			p->p_flag |= P_STOPPED_SIG;
			sigqueue_delete(sigqueue, sig);
			return (0);
		}

		/*
		 * All other kinds of signals:
		 * If a thread is sleeping interruptibly, simulate a
		 * wakeup so that when it is continued it will be made
		 * runnable and can look at the signal.  However, don't make
		 * the PROCESS runnable, leave it stopped.
		 * It may run a bit until it hits a thread_suspend_check().
		 */
		PROC_SLOCK(p);
		thread_lock(td);
		if (TD_CAN_ABORT(td))
			sig_sleepq_abort(td, intrval);
		else
			thread_unlock(td);
		PROC_SUNLOCK(p);
		return (0);
		/*
		 * Mutexes are short lived. Threads waiting on them will
		 * hit thread_suspend_check() soon.
		 */
	} else if (p->p_state == PRS_NORMAL) {
		if (p->p_flag & P_TRACED || action == SIG_CATCH) {
			tdsigwakeup(td, sig, action, intrval);
			return (0);
		}

		MPASS(action == SIG_DFL);

		if (prop & SIGPROP_STOP) {
			if (p->p_flag & (P_PPWAIT|P_WEXIT))
				return (0);
			p->p_flag |= P_STOPPED_SIG;
			p->p_xsig = sig;
			PROC_SLOCK(p);
			sig_suspend_threads(td, p);
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
				sigqueue_delete_proc(p, p->p_xsig);
			} else
				PROC_SUNLOCK(p);
			return (0);
		}
	} else {
		/* Not in "NORMAL" state. discard the signal. */
		sigqueue_delete(sigqueue, sig);
		return (0);
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
out_cont:
	itimer_proc_continue(p);
	kqtimer_proc_continue(p);

	return (0);
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
	int prop;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	prop = sigprop(sig);

	PROC_SLOCK(p);
	thread_lock(td);
	/*
	 * Bring the priority of a thread up if we want it to get
	 * killed in this lifetime.  Be careful to avoid bumping the
	 * priority of the idle thread, since we still allow to signal
	 * kernel processes.
	 */
	if (action == SIG_DFL && (prop & SIGPROP_KILL) != 0 &&
	    td->td_priority > PUSER && !TD_IS_IDLETHREAD(td))
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
		if ((prop & SIGPROP_CONT) && action == SIG_DFL) {
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
		 * Don't awaken a sleeping thread for SIGSTOP if the
		 * STOP signal is deferred.
		 */
		if ((prop & SIGPROP_STOP) != 0 && (td->td_flags & (TDF_SBDRY |
		    TDF_SERESTART | TDF_SEINTR)) == TDF_SBDRY)
			goto out;

		/*
		 * Give low priority threads a better chance to run.
		 */
		if (td->td_priority > PUSER && !TD_IS_IDLETHREAD(td))
			sched_prio(td, PUSER);

		sig_sleepq_abort(td, intrval);
		PROC_SUNLOCK(p);
		return;
	}

	/*
	 * Other states do nothing with the signal immediately,
	 * other than kicking ourselves if we are running.
	 * It will either never be noticed, or noticed very soon.
	 */
#ifdef SMP
	if (TD_IS_RUNNING(td) && td != curthread)
		forward_signal(td);
#endif

out:
	PROC_SUNLOCK(p);
	thread_unlock(td);
}

static void
ptrace_coredumpreq(struct thread *td, struct proc *p,
    struct thr_coredump_req *tcq)
{
	void *rl_cookie;

	if (p->p_sysent->sv_coredump == NULL) {
		tcq->tc_error = ENOSYS;
		return;
	}

	rl_cookie = vn_rangelock_wlock(tcq->tc_vp, 0, OFF_MAX);
	tcq->tc_error = p->p_sysent->sv_coredump(td, tcq->tc_vp,
	    tcq->tc_limit, tcq->tc_flags);
	vn_rangelock_unlock(tcq->tc_vp, rl_cookie);
}

static void
ptrace_syscallreq(struct thread *td, struct proc *p,
    struct thr_syscall_req *tsr)
{
	struct sysentvec *sv;
	struct sysent *se;
	register_t rv_saved[2];
	int error, nerror;
	int sc;
	bool audited, sy_thr_static;

	sv = p->p_sysent;
	if (sv->sv_table == NULL || sv->sv_size < tsr->ts_sa.code) {
		tsr->ts_ret.sr_error = ENOSYS;
		return;
	}

	sc = tsr->ts_sa.code;
	if (sc == SYS_syscall || sc == SYS___syscall) {
		sc = tsr->ts_sa.args[0];
		memmove(&tsr->ts_sa.args[0], &tsr->ts_sa.args[1],
		    sizeof(register_t) * (tsr->ts_nargs - 1));
	}

	tsr->ts_sa.callp = se = &sv->sv_table[sc];

	VM_CNT_INC(v_syscall);
	td->td_pticks = 0;
	if (__predict_false(td->td_cowgen != atomic_load_int(
	    &td->td_proc->p_cowgen)))
		thread_cow_update(td);

	td->td_sa = tsr->ts_sa;

#ifdef CAPABILITY_MODE
	if ((se->sy_flags & SYF_CAPENABLED) == 0) {
		if (CAP_TRACING(td))
			ktrcapfail(CAPFAIL_SYSCALL, NULL);
		if (IN_CAPABILITY_MODE(td)) {
			tsr->ts_ret.sr_error = ECAPMODE;
			return;
		}
	}
#endif

	sy_thr_static = (se->sy_thrcnt & SY_THR_STATIC) != 0;
	audited = AUDIT_SYSCALL_ENTER(sc, td) != 0;

	if (!sy_thr_static) {
		error = syscall_thread_enter(td, &se);
		sy_thr_static = (se->sy_thrcnt & SY_THR_STATIC) != 0;
		if (error != 0) {
			tsr->ts_ret.sr_error = error;
			return;
		}
	}

	rv_saved[0] = td->td_retval[0];
	rv_saved[1] = td->td_retval[1];
	nerror = td->td_errno;
	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

#ifdef KDTRACE_HOOKS
	if (se->sy_entry != 0)
		(*systrace_probe_func)(&tsr->ts_sa, SYSTRACE_ENTRY, 0);
#endif
	tsr->ts_ret.sr_error = se->sy_call(td, tsr->ts_sa.args);
#ifdef KDTRACE_HOOKS
	if (se->sy_return != 0)
		(*systrace_probe_func)(&tsr->ts_sa, SYSTRACE_RETURN,
		    tsr->ts_ret.sr_error != 0 ? -1 : td->td_retval[0]);
#endif

	tsr->ts_ret.sr_retval[0] = td->td_retval[0];
	tsr->ts_ret.sr_retval[1] = td->td_retval[1];
	td->td_retval[0] = rv_saved[0];
	td->td_retval[1] = rv_saved[1];
	td->td_errno = nerror;

	if (audited)
		AUDIT_SYSCALL_EXIT(error, td);
	if (!sy_thr_static)
		syscall_thread_exit(td, se);
}

static void
ptrace_remotereq(struct thread *td, int flag)
{
	struct proc *p;

	MPASS(td == curthread);
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((td->td_dbgflags & flag) == 0)
		return;
	KASSERT((p->p_flag & P_STOPPED_TRACE) != 0, ("not stopped"));
	KASSERT(td->td_remotereq != NULL, ("td_remotereq is NULL"));

	PROC_UNLOCK(p);
	switch (flag) {
	case TDB_COREDUMPREQ:
		ptrace_coredumpreq(td, p, td->td_remotereq);
		break;
	case TDB_SCREMOTEREQ:
		ptrace_syscallreq(td, p, td->td_remotereq);
		break;
	default:
		__unreachable();
	}
	PROC_LOCK(p);

	MPASS((td->td_dbgflags & flag) != 0);
	td->td_dbgflags &= ~flag;
	td->td_remotereq = NULL;
	wakeup(p);
}

/*
 * Suspend threads of the process p, either by directly setting the
 * inhibitor for the thread sleeping interruptibly, or by making the
 * thread suspend at the userspace boundary by scheduling a suspend AST.
 *
 * Returns true if some threads were suspended directly from the
 * sleeping state, and false if all threads are forced to process AST.
 */
static bool
sig_suspend_threads(struct thread *td, struct proc *p)
{
	struct thread *td2;
	bool res;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);

	res = false;
	FOREACH_THREAD_IN_PROC(p, td2) {
		thread_lock(td2);
		ast_sched_locked(td2, TDA_SUSPEND);
		if (TD_IS_SLEEPING(td2) && (td2->td_flags & TDF_SINTR) != 0) {
			if (td2->td_flags & TDF_SBDRY) {
				/*
				 * Once a thread is asleep with
				 * TDF_SBDRY and without TDF_SERESTART
				 * or TDF_SEINTR set, it should never
				 * become suspended due to this check.
				 */
				KASSERT(!TD_IS_SUSPENDED(td2),
				    ("thread with deferred stops suspended"));
				if (TD_SBDRY_INTR(td2)) {
					sleepq_abort(td2, TD_SBDRY_ERRNO(td2));
					continue;
				}
			} else if (!TD_IS_SUSPENDED(td2)) {
				thread_suspend_one(td2);
				res = true;
			}
		} else if (!TD_IS_SUSPENDED(td2)) {
#ifdef SMP
			if (TD_IS_RUNNING(td2) && td2 != td)
				forward_signal(td2);
#endif
		}
		thread_unlock(td2);
	}
	return (res);
}

static void
sig_handle_first_stop(struct thread *td, struct proc *p, int sig)
{
	if (td != NULL && (td->td_dbgflags & TDB_FSTP) == 0 &&
	    ((p->p_flag2 & P2_PTRACE_FSTP) != 0 || p->p_xthread != NULL))
		return;

	p->p_xsig = sig;
	p->p_xthread = td;

	/*
	 * If we are on sleepqueue already, let sleepqueue
	 * code decide if it needs to go sleep after attach.
	 */
	if (td != NULL && td->td_wchan == NULL)
		td->td_dbgflags &= ~TDB_FSTP;

	p->p_flag2 &= ~P2_PTRACE_FSTP;
	p->p_flag |= P_STOPPED_SIG | P_STOPPED_TRACE;
	if (sig_suspend_threads(td, p) && td == NULL)
		thread_stopped(p);
}

/*
 * Stop the process for an event deemed interesting to the debugger. If si is
 * non-NULL, this is a signal exchange; the new signal requested by the
 * debugger will be returned for handling. If si is NULL, this is some other
 * type of interesting event. The debugger may request a signal be delivered in
 * that case as well, however it will be deferred until it can be handled.
 */
int
ptracestop(struct thread *td, int sig, ksiginfo_t *si)
{
	struct proc *p = td->td_proc;
	struct thread *td2;
	ksiginfo_t ksi;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(!(p->p_flag & P_WEXIT), ("Stopping exiting process"));
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
	    &p->p_mtx.lock_object, "Stopping for traced signal");

	td->td_xsig = sig;

	if (si == NULL || (si->ksi_flags & KSI_PTRACE) == 0) {
		td->td_dbgflags |= TDB_XSIG;
		CTR4(KTR_PTRACE, "ptracestop: tid %d (pid %d) flags %#x sig %d",
		    td->td_tid, p->p_pid, td->td_dbgflags, sig);
		PROC_SLOCK(p);
		while ((p->p_flag & P_TRACED) && (td->td_dbgflags & TDB_XSIG)) {
			if (P_KILLED(p)) {
				/*
				 * Ensure that, if we've been PT_KILLed, the
				 * exit status reflects that. Another thread
				 * may also be in ptracestop(), having just
				 * received the SIGKILL, but this thread was
				 * unsuspended first.
				 */
				td->td_dbgflags &= ~TDB_XSIG;
				td->td_xsig = SIGKILL;
				p->p_ptevents = 0;
				break;
			}
			if (p->p_flag & P_SINGLE_EXIT &&
			    !(td->td_dbgflags & TDB_EXIT)) {
				/*
				 * Ignore ptrace stops except for thread exit
				 * events when the process exits.
				 */
				td->td_dbgflags &= ~TDB_XSIG;
				PROC_SUNLOCK(p);
				return (0);
			}

			/*
			 * Make wait(2) work.  Ensure that right after the
			 * attach, the thread which was decided to become the
			 * leader of attach gets reported to the waiter.
			 * Otherwise, just avoid overwriting another thread's
			 * assignment to p_xthread.  If another thread has
			 * already set p_xthread, the current thread will get
			 * a chance to report itself upon the next iteration.
			 */
			sig_handle_first_stop(td, p, sig);

			if ((td->td_dbgflags & TDB_STOPATFORK) != 0) {
				td->td_dbgflags &= ~TDB_STOPATFORK;
			}
stopme:
			td->td_dbgflags |= TDB_SSWITCH;
			thread_suspend_switch(td, p);
			td->td_dbgflags &= ~TDB_SSWITCH;
			if ((td->td_dbgflags & (TDB_COREDUMPREQ |
			    TDB_SCREMOTEREQ)) != 0) {
				MPASS((td->td_dbgflags & (TDB_COREDUMPREQ |
				    TDB_SCREMOTEREQ)) !=
				    (TDB_COREDUMPREQ | TDB_SCREMOTEREQ));
				PROC_SUNLOCK(p);
				ptrace_remotereq(td, td->td_dbgflags &
				    (TDB_COREDUMPREQ | TDB_SCREMOTEREQ));
				PROC_SLOCK(p);
				goto stopme;
			}
			if (p->p_xthread == td)
				p->p_xthread = NULL;
			if (!(p->p_flag & P_TRACED))
				break;
			if (td->td_dbgflags & TDB_SUSPEND) {
				if (p->p_flag & P_SINGLE_EXIT)
					break;
				goto stopme;
			}
		}
		PROC_SUNLOCK(p);
	}

	if (si != NULL && sig == td->td_xsig) {
		/* Parent wants us to take the original signal unchanged. */
		si->ksi_flags |= KSI_HEAD;
		if (sigqueue_add(&td->td_sigqueue, sig, si) != 0)
			si->ksi_signo = 0;
	} else if (td->td_xsig != 0) {
		/*
		 * If parent wants us to take a new signal, then it will leave
		 * it in td->td_xsig; otherwise we just look for signals again.
		 */
		ksiginfo_init(&ksi);
		ksi.ksi_signo = td->td_xsig;
		ksi.ksi_flags |= KSI_PTRACE;
		td2 = sigtd(p, td->td_xsig, false);
		tdsendsignal(p, td2, td->td_xsig, &ksi);
		if (td != td2)
			return (0);
	}

	return (td->td_xsig);
}

static void
reschedule_signals(struct proc *p, sigset_t block, int flags)
{
	struct sigacts *ps;
	struct thread *td;
	int sig;
	bool fastblk, pslocked;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	pslocked = (flags & SIGPROCMASK_PS_LOCKED) != 0;
	mtx_assert(&ps->ps_mtx, pslocked ? MA_OWNED : MA_NOTOWNED);
	if (SIGISEMPTY(p->p_siglist))
		return;
	SIGSETAND(block, p->p_siglist);
	fastblk = (flags & SIGPROCMASK_FASTBLK) != 0;
	SIG_FOREACH(sig, &block) {
		td = sigtd(p, sig, fastblk);

		/*
		 * If sigtd() selected us despite sigfastblock is
		 * blocking, do not activate AST or wake us, to avoid
		 * loop in AST handler.
		 */
		if (fastblk && td == curthread)
			continue;

		signotify(td);
		if (!pslocked)
			mtx_lock(&ps->ps_mtx);
		if (p->p_flag & P_TRACED ||
		    (SIGISMEMBER(ps->ps_sigcatch, sig) &&
		    !SIGISMEMBER(td->td_sigmask, sig))) {
			tdsigwakeup(td, sig, SIG_CATCH,
			    (SIGISMEMBER(ps->ps_sigintr, sig) ? EINTR :
			    ERESTART));
		}
		if (!pslocked)
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

static int
sigdeferstop_curr_flags(int cflags)
{

	MPASS((cflags & (TDF_SEINTR | TDF_SERESTART)) == 0 ||
	    (cflags & TDF_SBDRY) != 0);
	return (cflags & (TDF_SBDRY | TDF_SEINTR | TDF_SERESTART));
}

/*
 * Defer the delivery of SIGSTOP for the current thread, according to
 * the requested mode.  Returns previous flags, which must be restored
 * by sigallowstop().
 *
 * TDF_SBDRY, TDF_SEINTR, and TDF_SERESTART flags are only set and
 * cleared by the current thread, which allow the lock-less read-only
 * accesses below.
 */
int
sigdeferstop_impl(int mode)
{
	struct thread *td;
	int cflags, nflags;

	td = curthread;
	cflags = sigdeferstop_curr_flags(td->td_flags);
	switch (mode) {
	case SIGDEFERSTOP_NOP:
		nflags = cflags;
		break;
	case SIGDEFERSTOP_OFF:
		nflags = 0;
		break;
	case SIGDEFERSTOP_SILENT:
		nflags = (cflags | TDF_SBDRY) & ~(TDF_SEINTR | TDF_SERESTART);
		break;
	case SIGDEFERSTOP_EINTR:
		nflags = (cflags | TDF_SBDRY | TDF_SEINTR) & ~TDF_SERESTART;
		break;
	case SIGDEFERSTOP_ERESTART:
		nflags = (cflags | TDF_SBDRY | TDF_SERESTART) & ~TDF_SEINTR;
		break;
	default:
		panic("sigdeferstop: invalid mode %x", mode);
		break;
	}
	if (cflags == nflags)
		return (SIGDEFERSTOP_VAL_NCHG);
	thread_lock(td);
	td->td_flags = (td->td_flags & ~cflags) | nflags;
	thread_unlock(td);
	return (cflags);
}

/*
 * Restores the STOP handling mode, typically permitting the delivery
 * of SIGSTOP for the current thread.  This does not immediately
 * suspend if a stop was posted.  Instead, the thread will suspend
 * either via ast() or a subsequent interruptible sleep.
 */
void
sigallowstop_impl(int prev)
{
	struct thread *td;
	int cflags;

	KASSERT(prev != SIGDEFERSTOP_VAL_NCHG, ("failed sigallowstop"));
	KASSERT((prev & ~(TDF_SBDRY | TDF_SEINTR | TDF_SERESTART)) == 0,
	    ("sigallowstop: incorrect previous mode %x", prev));
	td = curthread;
	cflags = sigdeferstop_curr_flags(td->td_flags);
	if (cflags != prev) {
		thread_lock(td);
		td->td_flags = (td->td_flags & ~cflags) | prev;
		thread_unlock(td);
	}
}

enum sigstatus {
	SIGSTATUS_HANDLE,
	SIGSTATUS_HANDLED,
	SIGSTATUS_IGNORE,
	SIGSTATUS_SBDRY_STOP,
};

/*
 * The thread has signal "sig" pending.  Figure out what to do with it:
 *
 * _HANDLE     -> the caller should handle the signal
 * _HANDLED    -> handled internally, reload pending signal set
 * _IGNORE     -> ignored, remove from the set of pending signals and try the
 *                next pending signal
 * _SBDRY_STOP -> the signal should stop the thread but this is not
 *                permitted in the current context
 */
static enum sigstatus
sigprocess(struct thread *td, int sig)
{
	struct proc *p;
	struct sigacts *ps;
	struct sigqueue *queue;
	ksiginfo_t ksi;
	int prop;

	KASSERT(_SIG_VALID(sig), ("%s: invalid signal %d", __func__, sig));

	p = td->td_proc;
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * We should allow pending but ignored signals below
	 * if there is sigwait() active, or P_TRACED was
	 * on when they were posted.
	 */
	if (SIGISMEMBER(ps->ps_sigignore, sig) &&
	    (p->p_flag & P_TRACED) == 0 &&
	    (td->td_flags & TDF_SIGWAIT) == 0) {
		return (SIGSTATUS_IGNORE);
	}

	/*
	 * If the process is going to single-thread mode to prepare
	 * for exit, there is no sense in delivering any signal
	 * to usermode.  Another important consequence is that
	 * msleep(..., PCATCH, ...) now is only interruptible by a
	 * suspend request.
	 */
	if ((p->p_flag2 & P2_WEXIT) != 0)
		return (SIGSTATUS_IGNORE);

	if ((p->p_flag & (P_TRACED | P_PPTRACE)) == P_TRACED) {
		/*
		 * If traced, always stop.
		 * Remove old signal from queue before the stop.
		 * XXX shrug off debugger, it causes siginfo to
		 * be thrown away.
		 */
		queue = &td->td_sigqueue;
		ksiginfo_init(&ksi);
		if (sigqueue_get(queue, sig, &ksi) == 0) {
			queue = &p->p_sigqueue;
			sigqueue_get(queue, sig, &ksi);
		}
		td->td_si = ksi.ksi_info;

		mtx_unlock(&ps->ps_mtx);
		sig = ptracestop(td, sig, &ksi);
		mtx_lock(&ps->ps_mtx);

		td->td_si.si_signo = 0;

		/*
		 * Keep looking if the debugger discarded or
		 * replaced the signal.
		 */
		if (sig == 0)
			return (SIGSTATUS_HANDLED);

		/*
		 * If the signal became masked, re-queue it.
		 */
		if (SIGISMEMBER(td->td_sigmask, sig)) {
			ksi.ksi_flags |= KSI_HEAD;
			sigqueue_add(&p->p_sigqueue, sig, &ksi);
			return (SIGSTATUS_HANDLED);
		}

		/*
		 * If the traced bit got turned off, requeue the signal and
		 * reload the set of pending signals.  This ensures that p_sig*
		 * and p_sigact are consistent.
		 */
		if ((p->p_flag & P_TRACED) == 0) {
			if ((ksi.ksi_flags & KSI_PTRACE) == 0) {
				ksi.ksi_flags |= KSI_HEAD;
				sigqueue_add(queue, sig, &ksi);
			}
			return (SIGSTATUS_HANDLED);
		}
	}

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
			return (SIGSTATUS_IGNORE);
		}

		/*
		 * If there is a pending stop signal to process with
		 * default action, stop here, then clear the signal.
		 * Traced or exiting processes should ignore stops.
		 * Additionally, a member of an orphaned process group
		 * should ignore tty stops.
		 */
		prop = sigprop(sig);
		if (prop & SIGPROP_STOP) {
			mtx_unlock(&ps->ps_mtx);
			if ((p->p_flag & (P_TRACED | P_WEXIT |
			    P_SINGLE_EXIT)) != 0 || ((p->p_pgrp->
			    pg_flags & PGRP_ORPHANED) != 0 &&
			    (prop & SIGPROP_TTYSTOP) != 0)) {
				mtx_lock(&ps->ps_mtx);
				return (SIGSTATUS_IGNORE);
			}
			if (TD_SBDRY_INTR(td)) {
				KASSERT((td->td_flags & TDF_SBDRY) != 0,
				    ("lost TDF_SBDRY"));
				mtx_lock(&ps->ps_mtx);
				return (SIGSTATUS_SBDRY_STOP);
			}
			WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
			    &p->p_mtx.lock_object, "Catching SIGSTOP");
			sigqueue_delete(&td->td_sigqueue, sig);
			sigqueue_delete(&p->p_sigqueue, sig);
			p->p_flag |= P_STOPPED_SIG;
			p->p_xsig = sig;
			PROC_SLOCK(p);
			sig_suspend_threads(td, p);
			thread_suspend_switch(td, p);
			PROC_SUNLOCK(p);
			mtx_lock(&ps->ps_mtx);
			return (SIGSTATUS_HANDLED);
		} else if ((prop & SIGPROP_IGNORE) != 0 &&
		    (td->td_flags & TDF_SIGWAIT) == 0) {
			/*
			 * Default action is to ignore; drop it if
			 * not in kern_sigtimedwait().
			 */
			return (SIGSTATUS_IGNORE);
		} else {
			return (SIGSTATUS_HANDLE);
		}

	case (intptr_t)SIG_IGN:
		if ((td->td_flags & TDF_SIGWAIT) == 0)
			return (SIGSTATUS_IGNORE);
		else
			return (SIGSTATUS_HANDLE);

	default:
		/*
		 * This signal has an action, let postsig() process it.
		 */
		return (SIGSTATUS_HANDLE);
	}
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling
 * issignal by checking the pending signal masks in cursig.) The normal call
 * sequence is
 *
 *	while (sig = cursig(curthread))
 *		postsig(sig);
 */
static int
issignal(struct thread *td)
{
	struct proc *p;
	sigset_t sigpending;
	int sig;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	for (;;) {
		sigpending = td->td_sigqueue.sq_signals;
		SIGSETOR(sigpending, p->p_sigqueue.sq_signals);
		SIGSETNAND(sigpending, td->td_sigmask);

		if ((p->p_flag & P_PPWAIT) != 0 || (td->td_flags &
		    (TDF_SBDRY | TDF_SERESTART | TDF_SEINTR)) == TDF_SBDRY)
			SIG_STOPSIGMASK(sigpending);
		if (SIGISEMPTY(sigpending))	/* no signal to send */
			return (0);

		/*
		 * Do fast sigblock if requested by usermode.  Since
		 * we do know that there was a signal pending at this
		 * point, set the FAST_SIGBLOCK_PEND as indicator for
		 * usermode to perform a dummy call to
		 * FAST_SIGBLOCK_UNBLOCK, which causes immediate
		 * delivery of postponed pending signal.
		 */
		if ((td->td_pflags & TDP_SIGFASTBLOCK) != 0) {
			if (td->td_sigblock_val != 0)
				SIGSETNAND(sigpending, fastblock_mask);
			if (SIGISEMPTY(sigpending)) {
				td->td_pflags |= TDP_SIGFASTPENDING;
				return (0);
			}
		}

		if (!pt_attach_transparent &&
		    (p->p_flag & (P_TRACED | P_PPTRACE)) == P_TRACED &&
		    (p->p_flag2 & P2_PTRACE_FSTP) != 0 &&
		    SIGISMEMBER(sigpending, SIGSTOP)) {
			/*
			 * If debugger just attached, always consume
			 * SIGSTOP from ptrace(PT_ATTACH) first, to
			 * execute the debugger attach ritual in
			 * order.
			 */
			td->td_dbgflags |= TDB_FSTP;
			SIGEMPTYSET(sigpending);
			SIGADDSET(sigpending, SIGSTOP);
		}

		SIG_FOREACH(sig, &sigpending) {
			switch (sigprocess(td, sig)) {
			case SIGSTATUS_HANDLE:
				return (sig);
			case SIGSTATUS_HANDLED:
				goto next;
			case SIGSTATUS_IGNORE:
				sigqueue_delete(&td->td_sigqueue, sig);
				sigqueue_delete(&p->p_sigqueue, sig);
				break;
			case SIGSTATUS_SBDRY_STOP:
				return (-1);
			}
		}
next:;
	}
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
postsig(int sig)
{
	struct thread *td;
	struct proc *p;
	struct sigacts *ps;
	sig_t action;
	ksiginfo_t ksi;
	sigset_t returnmask;

	KASSERT(sig != 0, ("postsig"));

	td = curthread;
	p = td->td_proc;
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
		    &td->td_oldsigmask : &td->td_sigmask, ksi.ksi_code);
#endif

	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		mtx_unlock(&ps->ps_mtx);
		proc_td_siginfo_capture(td, &ksi.ksi_info);
		sigexit(td, sig);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
		KASSERT(action != SIG_IGN, ("postsig action %p", action));
		KASSERT(!SIGISMEMBER(td->td_sigmask, sig),
		    ("postsig action: blocked sig %d", sig));

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

		if (p->p_sig == sig) {
			p->p_sig = 0;
		}
		(*p->p_sysent->sv_sendsig)(action, &ksi, &returnmask);
		postsig_done(sig, td, ps);
	}
	return (1);
}

int
sig_ast_checksusp(struct thread *td)
{
	struct proc *p __diagused;
	int ret;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (!td_ast_pending(td, TDA_SUSPEND))
		return (0);

	ret = thread_suspend_check(1);
	MPASS(ret == 0 || ret == EINTR || ret == ERESTART);
	return (ret);
}

int
sig_ast_needsigchk(struct thread *td)
{
	struct proc *p;
	struct sigacts *ps;
	int ret, sig;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (!td_ast_pending(td, TDA_SIG))
		return (0);

	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	sig = cursig(td);
	if (sig == -1) {
		mtx_unlock(&ps->ps_mtx);
		KASSERT((td->td_flags & TDF_SBDRY) != 0, ("lost TDF_SBDRY"));
		KASSERT(TD_SBDRY_INTR(td),
		    ("lost TDF_SERESTART of TDF_SEINTR"));
		KASSERT((td->td_flags & (TDF_SEINTR | TDF_SERESTART)) !=
		    (TDF_SEINTR | TDF_SERESTART),
		    ("both TDF_SEINTR and TDF_SERESTART"));
		ret = TD_SBDRY_ERRNO(td);
	} else if (sig != 0) {
		ret = SIGISMEMBER(ps->ps_sigintr, sig) ? EINTR : ERESTART;
		mtx_unlock(&ps->ps_mtx);
	} else {
		mtx_unlock(&ps->ps_mtx);
		ret = 0;
	}

	/*
	 * Do not go into sleep if this thread was the ptrace(2)
	 * attach leader.  cursig() consumed SIGSTOP from PT_ATTACH,
	 * but we usually act on the signal by interrupting sleep, and
	 * should do that here as well.
	 */
	if ((td->td_dbgflags & TDB_FSTP) != 0) {
		if (ret == 0)
			ret = EINTR;
		td->td_dbgflags &= ~TDB_FSTP;
	}

	return (ret);
}

int
sig_intr(void)
{
	struct thread *td;
	struct proc *p;
	int ret;

	td = curthread;
	if (!td_ast_pending(td, TDA_SIG) && !td_ast_pending(td, TDA_SUSPEND))
		return (0);

	p = td->td_proc;

	PROC_LOCK(p);
	ret = sig_ast_checksusp(td);
	if (ret == 0)
		ret = sig_ast_needsigchk(td);
	PROC_UNLOCK(p);
	return (ret);
}

bool
curproc_sigkilled(void)
{
	struct thread *td;
	struct proc *p;
	struct sigacts *ps;
	bool res;

	td = curthread;
	if (!td_ast_pending(td, TDA_SIG))
		return (false);

	p = td->td_proc;
	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	res = SIGISMEMBER(td->td_sigqueue.sq_signals, SIGKILL) ||
	    SIGISMEMBER(p->p_sigqueue.sq_signals, SIGKILL);
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
	return (res);
}

void
proc_wkilled(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((p->p_flag & P_WKILLED) == 0)
		p->p_flag |= P_WKILLED;
}

/*
 * Kill the current process for stated reason.
 */
void
killproc(struct proc *p, const char *why)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	CTR3(KTR_PROC, "killproc: proc %p (pid %d, %s)", p, p->p_pid,
	    p->p_comm);
	log(LOG_ERR, "pid %d (%s), jid %d, uid %d, was killed: %s\n",
	    p->p_pid, p->p_comm, p->p_ucred->cr_prison->pr_id,
	    p->p_ucred->cr_uid, why);
	proc_wkilled(p);
	kern_psignal(p, SIGKILL);
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
sigexit(struct thread *td, int sig)
{
	struct proc *p = td->td_proc;
	const char *coreinfo;
	int rv;
	bool logexit;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	proc_set_p2_wexit(p);

	p->p_acflag |= AXSIG;
	if ((p->p_flag2 & P2_LOGSIGEXIT_CTL) == 0)
		logexit = kern_logsigexit != 0;
	else
		logexit = (p->p_flag2 & P2_LOGSIGEXIT_ENABLE) != 0;

	/*
	 * We must be single-threading to generate a core dump.  This
	 * ensures that the registers in the core file are up-to-date.
	 * Also, the ELF dump handler assumes that the thread list doesn't
	 * change out from under it.
	 *
	 * XXX If another thread attempts to single-thread before us
	 *     (e.g. via fork()), we won't get a dump at all.
	 */
	if ((sigprop(sig) & SIGPROP_CORE) &&
	    thread_single(p, SINGLE_NO_EXIT) == 0) {
		p->p_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 * Note that coredump() drops proc lock.
		 */
		rv = coredump(td);
		switch (rv) {
		case 0:
			sig |= WCOREFLAG;
			coreinfo = " (core dumped)";
			break;
		case EFAULT:
			coreinfo = " (no core dump - bad address)";
			break;
		case EINVAL:
			coreinfo = " (no core dump - invalid argument)";
			break;
		case EFBIG:
			coreinfo = " (no core dump - too large)";
			break;
		default:
			coreinfo = " (no core dump - other error)";
			break;
		}
		if (logexit)
			log(LOG_INFO,
			    "pid %d (%s), jid %d, uid %d: exited on "
			    "signal %d%s\n", p->p_pid, p->p_comm,
			    p->p_ucred->cr_prison->pr_id,
			    td->td_ucred->cr_uid,
			    sig &~ WCOREFLAG, coreinfo);
	} else
		PROC_UNLOCK(p);
	exit1(td, 0, sig);
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
	pksignal(p->p_pptr, SIGCHLD, p->p_ksi);
}

static void
childproc_jobstate(struct proc *p, int reason, int sig)
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
		sigparent(p, reason, sig);
	} else
		mtx_unlock(&ps->ps_mtx);
}

void
childproc_stopped(struct proc *p, int reason)
{

	childproc_jobstate(p, reason, p->p_xsig);
}

void
childproc_continued(struct proc *p)
{
	childproc_jobstate(p, CLD_CONTINUED, SIGCONT);
}

void
childproc_exited(struct proc *p)
{
	int reason, status;

	if (WCOREDUMP(p->p_xsig)) {
		reason = CLD_DUMPED;
		status = WTERMSIG(p->p_xsig);
	} else if (WIFSIGNALED(p->p_xsig)) {
		reason = CLD_KILLED;
		status = WTERMSIG(p->p_xsig);
	} else {
		reason = CLD_EXITED;
		status = p->p_xexit;
	}
	/*
	 * XXX avoid calling wakeup(p->p_pptr), the work is
	 * done in exit1().
	 */
	sigparent(p, reason, status);
}

#define	MAX_NUM_CORE_FILES 100000
#ifndef NUM_CORE_FILES
#define	NUM_CORE_FILES 5
#endif
CTASSERT(NUM_CORE_FILES >= 0 && NUM_CORE_FILES <= MAX_NUM_CORE_FILES);
static int num_cores = NUM_CORE_FILES;

static int
sysctl_debug_num_cores_check (SYSCTL_HANDLER_ARGS)
{
	int error;
	int new_val;

	new_val = num_cores;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val > MAX_NUM_CORE_FILES)
		new_val = MAX_NUM_CORE_FILES;
	if (new_val < 0)
		new_val = 0;
	num_cores = new_val;
	return (0);
}
SYSCTL_PROC(_debug, OID_AUTO, ncores,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, 0, sizeof(int),
    sysctl_debug_num_cores_check, "I",
    "Maximum number of generated process corefiles while using index format");

#define	GZIP_SUFFIX	".gz"
#define	ZSTD_SUFFIX	".zst"

int compress_user_cores = 0;

static int
sysctl_compress_user_cores(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = compress_user_cores;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0 && !compressor_avail(val))
		return (EINVAL);
	compress_user_cores = val;
	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, compress_user_cores,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT, 0, sizeof(int),
    sysctl_compress_user_cores, "I",
    "Enable compression of user corefiles ("
    __XSTRING(COMPRESS_GZIP) " = gzip, "
    __XSTRING(COMPRESS_ZSTD) " = zstd)");

int compress_user_cores_level = 6;
SYSCTL_INT(_kern, OID_AUTO, compress_user_cores_level, CTLFLAG_RWTUN,
    &compress_user_cores_level, 0,
    "Corefile compression level");

/*
 * Protect the access to corefilename[] by allproc_lock.
 */
#define	corefilename_lock	allproc_lock

static char corefilename[MAXPATHLEN] = {"%N.core"};
TUNABLE_STR("kern.corefile", corefilename, sizeof(corefilename));

static int
sysctl_kern_corefile(SYSCTL_HANDLER_ARGS)
{
	int error;

	sx_xlock(&corefilename_lock);
	error = sysctl_handle_string(oidp, corefilename, sizeof(corefilename),
	    req);
	sx_xunlock(&corefilename_lock);

	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, corefile, CTLTYPE_STRING | CTLFLAG_RW |
    CTLFLAG_MPSAFE, 0, 0, sysctl_kern_corefile, "A",
    "Process corefile name format string");

static void
vnode_close_locked(struct thread *td, struct vnode *vp)
{

	VOP_UNLOCK(vp);
	vn_close(vp, FWRITE, td->td_ucred, td);
}

/*
 * If the core format has a %I in it, then we need to check
 * for existing corefiles before defining a name.
 * To do this we iterate over 0..ncores to find a
 * non-existing core file name to use. If all core files are
 * already used we choose the oldest one.
 */
static int
corefile_open_last(struct thread *td, char *name, int indexpos,
    int indexlen, int ncores, struct vnode **vpp)
{
	struct vnode *oldvp, *nextvp, *vp;
	struct vattr vattr;
	struct nameidata nd;
	int error, i, flags, oflags, cmode;
	char ch;
	struct timespec lasttime;

	nextvp = oldvp = NULL;
	cmode = S_IRUSR | S_IWUSR;
	oflags = VN_OPEN_NOAUDIT | VN_OPEN_NAMECACHE |
	    (capmode_coredump ? VN_OPEN_NOCAPCHECK : 0);

	for (i = 0; i < ncores; i++) {
		flags = O_CREAT | FWRITE | O_NOFOLLOW;

		ch = name[indexpos + indexlen];
		(void)snprintf(name + indexpos, indexlen + 1, "%.*u", indexlen,
		    i);
		name[indexpos + indexlen] = ch;

		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name);
		error = vn_open_cred(&nd, &flags, cmode, oflags, td->td_ucred,
		    NULL);
		if (error != 0)
			break;

		vp = nd.ni_vp;
		NDFREE_PNBUF(&nd);
		if ((flags & O_CREAT) == O_CREAT) {
			nextvp = vp;
			break;
		}

		error = VOP_GETATTR(vp, &vattr, td->td_ucred);
		if (error != 0) {
			vnode_close_locked(td, vp);
			break;
		}

		if (oldvp == NULL ||
		    lasttime.tv_sec > vattr.va_mtime.tv_sec ||
		    (lasttime.tv_sec == vattr.va_mtime.tv_sec &&
		    lasttime.tv_nsec >= vattr.va_mtime.tv_nsec)) {
			if (oldvp != NULL)
				vn_close(oldvp, FWRITE, td->td_ucred, td);
			oldvp = vp;
			VOP_UNLOCK(oldvp);
			lasttime = vattr.va_mtime;
		} else {
			vnode_close_locked(td, vp);
		}
	}

	if (oldvp != NULL) {
		if (nextvp == NULL) {
			if ((td->td_proc->p_flag & P_SUGID) != 0) {
				error = EFAULT;
				vn_close(oldvp, FWRITE, td->td_ucred, td);
			} else {
				nextvp = oldvp;
				error = vn_lock(nextvp, LK_EXCLUSIVE);
				if (error != 0) {
					vn_close(nextvp, FWRITE, td->td_ucred,
					    td);
					nextvp = NULL;
				}
			}
		} else {
			vn_close(oldvp, FWRITE, td->td_ucred, td);
		}
	}
	if (error != 0) {
		if (nextvp != NULL)
			vnode_close_locked(td, oldvp);
	} else {
		*vpp = nextvp;
	}

	return (error);
}

/*
 * corefile_open(comm, uid, pid, td, compress, vpp, namep)
 * Expand the name described in corefilename, using name, uid, and pid
 * and open/create core file.
 * corefilename is a printf-like string, with three format specifiers:
 *	%N	name of process ("name")
 *	%P	process id (pid)
 *	%U	user id (uid)
 * For example, "%N.core" is the default; they can be disabled completely
 * by using "/dev/null", or all core files can be stored in "/cores/%U/%N-%P".
 * This is controlled by the sysctl variable kern.corefile (see above).
 */
static int
corefile_open(const char *comm, uid_t uid, pid_t pid, struct thread *td,
    int compress, int signum, struct vnode **vpp, char **namep)
{
	struct sbuf sb;
	struct nameidata nd;
	const char *format;
	char *hostname, *name;
	int cmode, error, flags, i, indexpos, indexlen, oflags, ncores;

	hostname = NULL;
	format = corefilename;
	name = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
	indexlen = 0;
	indexpos = -1;
	ncores = num_cores;
	(void)sbuf_new(&sb, name, MAXPATHLEN, SBUF_FIXEDLEN);
	sx_slock(&corefilename_lock);
	for (i = 0; format[i] != '\0'; i++) {
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				sbuf_putc(&sb, '%');
				break;
			case 'H':	/* hostname */
				if (hostname == NULL) {
					hostname = malloc(MAXHOSTNAMELEN,
					    M_TEMP, M_WAITOK);
				}
				getcredhostname(td->td_ucred, hostname,
				    MAXHOSTNAMELEN);
				sbuf_cat(&sb, hostname);
				break;
			case 'I':	/* autoincrementing index */
				if (indexpos != -1) {
					sbuf_printf(&sb, "%%I");
					break;
				}

				indexpos = sbuf_len(&sb);
				sbuf_printf(&sb, "%u", ncores - 1);
				indexlen = sbuf_len(&sb) - indexpos;
				break;
			case 'N':	/* process name */
				sbuf_printf(&sb, "%s", comm);
				break;
			case 'P':	/* process id */
				sbuf_printf(&sb, "%u", pid);
				break;
			case 'S':	/* signal number */
				sbuf_printf(&sb, "%i", signum);
				break;
			case 'U':	/* user id */
				sbuf_printf(&sb, "%u", uid);
				break;
			default:
				log(LOG_ERR,
				    "Unknown format character %c in "
				    "corename `%s'\n", format[i], format);
				break;
			}
			break;
		default:
			sbuf_putc(&sb, format[i]);
			break;
		}
	}
	sx_sunlock(&corefilename_lock);
	free(hostname, M_TEMP);
	if (compress == COMPRESS_GZIP)
		sbuf_cat(&sb, GZIP_SUFFIX);
	else if (compress == COMPRESS_ZSTD)
		sbuf_cat(&sb, ZSTD_SUFFIX);
	if (sbuf_error(&sb) != 0) {
		log(LOG_ERR, "pid %ld (%s), uid (%lu): corename is too "
		    "long\n", (long)pid, comm, (u_long)uid);
		sbuf_delete(&sb);
		free(name, M_TEMP);
		return (ENOMEM);
	}
	sbuf_finish(&sb);
	sbuf_delete(&sb);

	if (indexpos != -1) {
		error = corefile_open_last(td, name, indexpos, indexlen, ncores,
		    vpp);
		if (error != 0) {
			log(LOG_ERR,
			    "pid %d (%s), uid (%u):  Path `%s' failed "
			    "on initial open test, error = %d\n",
			    pid, comm, uid, name, error);
		}
	} else {
		cmode = S_IRUSR | S_IWUSR;
		oflags = VN_OPEN_NOAUDIT | VN_OPEN_NAMECACHE |
		    (capmode_coredump ? VN_OPEN_NOCAPCHECK : 0);
		flags = O_CREAT | FWRITE | O_NOFOLLOW;
		if ((td->td_proc->p_flag & P_SUGID) != 0)
			flags |= O_EXCL;

		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name);
		error = vn_open_cred(&nd, &flags, cmode, oflags, td->td_ucred,
		    NULL);
		if (error == 0) {
			*vpp = nd.ni_vp;
			NDFREE_PNBUF(&nd);
		}
	}

	if (error != 0) {
#ifdef AUDIT
		audit_proc_coredump(td, name, error);
#endif
		free(name, M_TEMP);
		return (error);
	}
	*namep = name;
	return (0);
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
	struct ucred *cred = td->td_ucred;
	struct vnode *vp;
	struct flock lf;
	struct vattr vattr;
	size_t fullpathsize;
	int error, error1, locked;
	char *name;			/* name of corefile */
	void *rl_cookie;
	off_t limit;
	char *fullpath, *freepath = NULL;
	struct sbuf *sb;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS((p->p_flag & P_HADTHREADS) == 0 || p->p_singlethread == td);

	if (!do_coredump || (!sugid_coredump && (p->p_flag & P_SUGID) != 0) ||
	    (p->p_flag2 & P2_NOTRACE) != 0) {
		PROC_UNLOCK(p);
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
	limit = (off_t)lim_cur(td, RLIMIT_CORE);
	if (limit == 0 || racct_get_available(p, RACCT_CORE) == 0) {
		PROC_UNLOCK(p);
		return (EFBIG);
	}
	PROC_UNLOCK(p);

	error = corefile_open(p->p_comm, cred->cr_uid, p->p_pid, td,
	    compress_user_cores, p->p_sig, &vp, &name);
	if (error != 0)
		return (error);

	/*
	 * Don't dump to non-regular files or files with links.
	 * Do not dump into system files. Effective user must own the corefile.
	 */
	if (vp->v_type != VREG || VOP_GETATTR(vp, &vattr, cred) != 0 ||
	    vattr.va_nlink != 1 || (vp->v_vflag & VV_SYSTEM) != 0 ||
	    vattr.va_uid != cred->cr_uid) {
		VOP_UNLOCK(vp);
		error = EFAULT;
		goto out;
	}

	VOP_UNLOCK(vp);

	/* Postpone other writers, including core dumps of other processes. */
	rl_cookie = vn_rangelock_wlock(vp, 0, OFF_MAX);

	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	lf.l_type = F_WRLCK;
	locked = (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &lf, F_FLOCK) == 0);

	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	if (set_core_nodump_flag)
		vattr.va_flags = UF_NODUMP;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_SETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp);
	PROC_LOCK(p);
	p->p_acflag |= ACORE;
	PROC_UNLOCK(p);

	if (p->p_sysent->sv_coredump != NULL) {
		error = p->p_sysent->sv_coredump(td, vp, limit, 0);
	} else {
		error = ENOSYS;
	}

	if (locked) {
		lf.l_type = F_UNLCK;
		VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
	}
	vn_rangelock_unlock(vp, rl_cookie);

	/*
	 * Notify the userland helper that a process triggered a core dump.
	 * This allows the helper to run an automated debugging session.
	 */
	if (error != 0 || coredump_devctl == 0)
		goto out;
	sb = sbuf_new_auto();
	if (vn_fullpath_global(p->p_textvp, &fullpath, &freepath) != 0)
		goto out2;
	sbuf_cat(sb, "comm=\"");
	devctl_safe_quote_sb(sb, fullpath);
	free(freepath, M_TEMP);
	sbuf_cat(sb, "\" core=\"");

	/*
	 * We can't lookup core file vp directly. When we're replacing a core, and
	 * other random times, we flush the name cache, so it will fail. Instead,
	 * if the path of the core is relative, add the current dir in front if it.
	 */
	if (name[0] != '/') {
		fullpathsize = MAXPATHLEN;
		freepath = malloc(fullpathsize, M_TEMP, M_WAITOK);
		if (vn_getcwd(freepath, &fullpath, &fullpathsize) != 0) {
			free(freepath, M_TEMP);
			goto out2;
		}
		devctl_safe_quote_sb(sb, fullpath);
		free(freepath, M_TEMP);
		sbuf_putc(sb, '/');
	}
	devctl_safe_quote_sb(sb, name);
	sbuf_putc(sb, '"');
	if (sbuf_finish(sb) == 0)
		devctl_notify("kernel", "signal", "coredump", sbuf_data(sb));
out2:
	sbuf_delete(sb);
out:
	error1 = vn_close(vp, FWRITE, cred, td);
	if (error == 0)
		error = error1;
#ifdef AUDIT
	audit_proc_coredump(td, name, error);
#endif
	free(name, M_TEMP);
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
nosys(struct thread *td, struct nosys_args *args)
{
	struct proc *p;

	p = td->td_proc;

	if (SV_PROC_FLAG(p, SV_SIGSYS) != 0 && kern_signosys) {
		PROC_LOCK(p);
		tdsignal(td, SIGSYS);
		PROC_UNLOCK(p);
	}
	if (kern_lognosys == 1 || kern_lognosys == 3) {
		uprintf("pid %d comm %s: nosys %d\n", p->p_pid, p->p_comm,
		    td->td_sa.code);
	}
	if (kern_lognosys == 2 || kern_lognosys == 3 ||
	    (p->p_pid == 1 && (kern_lognosys & 3) == 0)) {
		printf("pid %d comm %s: nosys %d\n", p->p_pid, p->p_comm,
		    td->td_sa.code);
	}
	return (ENOSYS);
}

/*
 * Send a SIGIO or SIGURG signal to a process or process group using stored
 * credentials rather than those of the current process.
 */
void
pgsigio(struct sigio **sigiop, int sig, int checkctty)
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
			kern_psignal(sigio->sio_proc, sig);
		PROC_UNLOCK(sigio->sio_proc);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		PGRP_LOCK(sigio->sio_pgrp);
		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NORMAL &&
			    CANSIGIO(sigio->sio_ucred, p->p_ucred) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				kern_psignal(p, sig);
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

	knlist_add(p->p_klist, kn, 0);

	return (0);
}

static void
filt_sigdetach(struct knote *kn)
{
	knlist_remove(kn->kn_knlist, kn, 0);
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
	refcount_init(&ps->ps_refcnt, 1);
	mtx_init(&ps->ps_mtx, "sigacts", NULL, MTX_DEF);
	return (ps);
}

void
sigacts_free(struct sigacts *ps)
{

	if (refcount_release(&ps->ps_refcnt) == 0)
		return;
	mtx_destroy(&ps->ps_mtx);
	free(ps, M_SUBPROC);
}

struct sigacts *
sigacts_hold(struct sigacts *ps)
{

	refcount_acquire(&ps->ps_refcnt);
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

	return (ps->ps_refcnt > 1);
}

void
sig_drop_caught(struct proc *p)
{
	int sig;
	struct sigacts *ps;

	ps = p->p_sigacts;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	SIG_FOREACH(sig, &ps->ps_sigcatch) {
		sigdflt(ps, sig);
		if ((sigprop(sig) & SIGPROP_IGNORE) != 0)
			sigqueue_delete_proc(p, sig);
	}
}

static void
sigfastblock_failed(struct thread *td, bool sendsig, bool write)
{
	ksiginfo_t ksi;

	/*
	 * Prevent further fetches and SIGSEGVs, allowing thread to
	 * issue syscalls despite corruption.
	 */
	sigfastblock_clear(td);

	if (!sendsig)
		return;
	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = SIGSEGV;
	ksi.ksi_code = write ? SEGV_ACCERR : SEGV_MAPERR;
	ksi.ksi_addr = td->td_sigblock_ptr;
	trapsignal(td, &ksi);
}

static bool
sigfastblock_fetch_sig(struct thread *td, bool sendsig, uint32_t *valp)
{
	uint32_t res;

	if ((td->td_pflags & TDP_SIGFASTBLOCK) == 0)
		return (true);
	if (fueword32((void *)td->td_sigblock_ptr, &res) == -1) {
		sigfastblock_failed(td, sendsig, false);
		return (false);
	}
	*valp = res;
	td->td_sigblock_val = res & ~SIGFASTBLOCK_FLAGS;
	return (true);
}

static void
sigfastblock_resched(struct thread *td, bool resched)
{
	struct proc *p;

	if (resched) {
		p = td->td_proc;
		PROC_LOCK(p);
		reschedule_signals(p, td->td_sigmask, 0);
		PROC_UNLOCK(p);
	}
	ast_sched(td, TDA_SIG);
}

int
sys_sigfastblock(struct thread *td, struct sigfastblock_args *uap)
{
	struct proc *p;
	int error, res;
	uint32_t oldval;

	error = 0;
	p = td->td_proc;
	switch (uap->cmd) {
	case SIGFASTBLOCK_SETPTR:
		if ((td->td_pflags & TDP_SIGFASTBLOCK) != 0) {
			error = EBUSY;
			break;
		}
		if (((uintptr_t)(uap->ptr) & (sizeof(uint32_t) - 1)) != 0) {
			error = EINVAL;
			break;
		}
		td->td_pflags |= TDP_SIGFASTBLOCK;
		td->td_sigblock_ptr = uap->ptr;
		break;

	case SIGFASTBLOCK_UNBLOCK:
		if ((td->td_pflags & TDP_SIGFASTBLOCK) == 0) {
			error = EINVAL;
			break;
		}

		for (;;) {
			res = casueword32(td->td_sigblock_ptr,
			    SIGFASTBLOCK_PEND, &oldval, 0);
			if (res == -1) {
				error = EFAULT;
				sigfastblock_failed(td, false, true);
				break;
			}
			if (res == 0)
				break;
			MPASS(res == 1);
			if (oldval != SIGFASTBLOCK_PEND) {
				error = EBUSY;
				break;
			}
			error = thread_check_susp(td, false);
			if (error != 0)
				break;
		}
		if (error != 0)
			break;

		/*
		 * td_sigblock_val is cleared there, but not on a
		 * syscall exit.  The end effect is that a single
		 * interruptible sleep, while user sigblock word is
		 * set, might return EINTR or ERESTART to usermode
		 * without delivering signal.  All further sleeps,
		 * until userspace clears the word and does
		 * sigfastblock(UNBLOCK), observe current word and no
		 * longer get interrupted.  It is slight
		 * non-conformance, with alternative to have read the
		 * sigblock word on each syscall entry.
		 */
		td->td_sigblock_val = 0;

		/*
		 * Rely on normal ast mechanism to deliver pending
		 * signals to current thread.  But notify others about
		 * fake unblock.
		 */
		sigfastblock_resched(td, error == 0 && p->p_numthreads != 1);

		break;

	case SIGFASTBLOCK_UNSETPTR:
		if ((td->td_pflags & TDP_SIGFASTBLOCK) == 0) {
			error = EINVAL;
			break;
		}
		if (!sigfastblock_fetch_sig(td, false, &oldval)) {
			error = EFAULT;
			break;
		}
		if (oldval != 0 && oldval != SIGFASTBLOCK_PEND) {
			error = EBUSY;
			break;
		}
		sigfastblock_clear(td);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

void
sigfastblock_clear(struct thread *td)
{
	bool resched;

	if ((td->td_pflags & TDP_SIGFASTBLOCK) == 0)
		return;
	td->td_sigblock_val = 0;
	resched = (td->td_pflags & TDP_SIGFASTPENDING) != 0 ||
	    SIGPENDING(td);
	td->td_pflags &= ~(TDP_SIGFASTBLOCK | TDP_SIGFASTPENDING);
	sigfastblock_resched(td, resched);
}

void
sigfastblock_fetch(struct thread *td)
{
	uint32_t val;

	(void)sigfastblock_fetch_sig(td, true, &val);
}

static void
sigfastblock_setpend1(struct thread *td)
{
	int res;
	uint32_t oldval;

	if ((td->td_pflags & TDP_SIGFASTPENDING) == 0)
		return;
	res = fueword32((void *)td->td_sigblock_ptr, &oldval);
	if (res == -1) {
		sigfastblock_failed(td, true, false);
		return;
	}
	for (;;) {
		res = casueword32(td->td_sigblock_ptr, oldval, &oldval,
		    oldval | SIGFASTBLOCK_PEND);
		if (res == -1) {
			sigfastblock_failed(td, true, true);
			return;
		}
		if (res == 0) {
			td->td_sigblock_val = oldval & ~SIGFASTBLOCK_FLAGS;
			td->td_pflags &= ~TDP_SIGFASTPENDING;
			break;
		}
		MPASS(res == 1);
		if (thread_check_susp(td, false) != 0)
			break;
	}
}

static void
sigfastblock_setpend(struct thread *td, bool resched)
{
	struct proc *p;

	sigfastblock_setpend1(td);
	if (resched) {
		p = td->td_proc;
		PROC_LOCK(p);
		reschedule_signals(p, fastblock_mask, SIGPROCMASK_FASTBLK);
		PROC_UNLOCK(p);
	}
}
