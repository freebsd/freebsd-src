/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
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

#include <sys/cdefs.h>
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <security/audit/audit.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_time.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_misc.h>

static int	linux_pksignal(struct thread *td, int pid, int sig,
		    ksiginfo_t *ksi);
static int	linux_psignal(struct thread *td, int pid, int sig);
static int	linux_tdksignal(struct thread *td, lwpid_t tid,
		    int tgid, int sig, ksiginfo_t *ksi);
static int	linux_tdsignal(struct thread *td, lwpid_t tid,
		    int tgid, int sig);
static void	sicode_to_lsicode(int sig, int si_code, int *lsi_code);
static int	linux_common_rt_sigtimedwait(struct thread *,
		    l_sigset_t *, struct timespec *, l_siginfo_t *,
		    l_size_t);

static void
linux_to_bsd_sigaction(l_sigaction_t *lsa, struct sigaction *bsa)
{
	unsigned long flags;

	linux_to_bsd_sigset(&lsa->lsa_mask, &bsa->sa_mask);
	bsa->sa_handler = PTRIN(lsa->lsa_handler);
	bsa->sa_flags = 0;

	flags = lsa->lsa_flags;
	if (lsa->lsa_flags & LINUX_SA_NOCLDSTOP) {
		flags &= ~LINUX_SA_NOCLDSTOP;
		bsa->sa_flags |= SA_NOCLDSTOP;
	}
	if (lsa->lsa_flags & LINUX_SA_NOCLDWAIT) {
		flags &= ~LINUX_SA_NOCLDWAIT;
		bsa->sa_flags |= SA_NOCLDWAIT;
	}
	if (lsa->lsa_flags & LINUX_SA_SIGINFO) {
		flags &= ~LINUX_SA_SIGINFO;
		bsa->sa_flags |= SA_SIGINFO;
#ifdef notyet
		/*
		 * XXX: We seem to be missing code to convert
		 *      some of the fields in ucontext_t.
		 */
		linux_msg(curthread,
		    "partially unsupported sigaction flag SA_SIGINFO");
#endif
	}
	if (lsa->lsa_flags & LINUX_SA_RESTORER) {
		flags &= ~LINUX_SA_RESTORER;
		/*
		 * We ignore the lsa_restorer and always use our own signal
		 * trampoline instead.  It looks like SA_RESTORER is obsolete
		 * in Linux too - it doesn't seem to be used at all on arm64.
		 * In any case: see Linux sigreturn(2).
		 */
	}
	if (lsa->lsa_flags & LINUX_SA_ONSTACK) {
		flags &= ~LINUX_SA_ONSTACK;
		bsa->sa_flags |= SA_ONSTACK;
	}
	if (lsa->lsa_flags & LINUX_SA_RESTART) {
		flags &= ~LINUX_SA_RESTART;
		bsa->sa_flags |= SA_RESTART;
	}
	if (lsa->lsa_flags & LINUX_SA_INTERRUPT) {
		flags &= ~LINUX_SA_INTERRUPT;
		/* Documented to be a "historical no-op". */
	}
	if (lsa->lsa_flags & LINUX_SA_ONESHOT) {
		flags &= ~LINUX_SA_ONESHOT;
		bsa->sa_flags |= SA_RESETHAND;
	}
	if (lsa->lsa_flags & LINUX_SA_NOMASK) {
		flags &= ~LINUX_SA_NOMASK;
		bsa->sa_flags |= SA_NODEFER;
	}

	if (flags != 0)
		linux_msg(curthread, "unsupported sigaction flag %#lx", flags);
}

static void
bsd_to_linux_sigaction(struct sigaction *bsa, l_sigaction_t *lsa)
{

	bsd_to_linux_sigset(&bsa->sa_mask, &lsa->lsa_mask);
#ifdef COMPAT_LINUX32
	lsa->lsa_handler = (uintptr_t)bsa->sa_handler;
#else
	lsa->lsa_handler = bsa->sa_handler;
#endif
	lsa->lsa_restorer = 0;		/* unsupported */
	lsa->lsa_flags = 0;
	if (bsa->sa_flags & SA_NOCLDSTOP)
		lsa->lsa_flags |= LINUX_SA_NOCLDSTOP;
	if (bsa->sa_flags & SA_NOCLDWAIT)
		lsa->lsa_flags |= LINUX_SA_NOCLDWAIT;
	if (bsa->sa_flags & SA_SIGINFO)
		lsa->lsa_flags |= LINUX_SA_SIGINFO;
	if (bsa->sa_flags & SA_ONSTACK)
		lsa->lsa_flags |= LINUX_SA_ONSTACK;
	if (bsa->sa_flags & SA_RESTART)
		lsa->lsa_flags |= LINUX_SA_RESTART;
	if (bsa->sa_flags & SA_RESETHAND)
		lsa->lsa_flags |= LINUX_SA_ONESHOT;
	if (bsa->sa_flags & SA_NODEFER)
		lsa->lsa_flags |= LINUX_SA_NOMASK;
}

int
linux_do_sigaction(struct thread *td, int linux_sig, l_sigaction_t *linux_nsa,
		   l_sigaction_t *linux_osa)
{
	struct sigaction act, oact, *nsa, *osa;
	int error, sig;

	if (!LINUX_SIG_VALID(linux_sig))
		return (EINVAL);

	osa = (linux_osa != NULL) ? &oact : NULL;
	if (linux_nsa != NULL) {
		nsa = &act;
		linux_to_bsd_sigaction(linux_nsa, nsa);
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&linux_nsa->lsa_mask,
			    sizeof(linux_nsa->lsa_mask));
#endif
	} else
		nsa = NULL;
	sig = linux_to_bsd_signal(linux_sig);

	error = kern_sigaction(td, sig, nsa, osa, 0);
	if (error != 0)
		return (error);

	if (linux_osa != NULL) {
		bsd_to_linux_sigaction(osa, linux_osa);
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&linux_osa->lsa_mask,
			    sizeof(linux_osa->lsa_mask));
#endif
	}
	return (0);
}

int
linux_sigaltstack(struct thread *td, struct linux_sigaltstack_args *uap)
{
	stack_t ss, oss;
	l_stack_t lss;
	int error;

	memset(&lss, 0, sizeof(lss));
	LINUX_CTR2(sigaltstack, "%p, %p", uap->uss, uap->uoss);

	if (uap->uss != NULL) {
		error = copyin(uap->uss, &lss, sizeof(lss));
		if (error != 0)
			return (error);

		ss.ss_sp = PTRIN(lss.ss_sp);
		ss.ss_size = lss.ss_size;
		ss.ss_flags = linux_to_bsd_sigaltstack(lss.ss_flags);
	}
	error = kern_sigaltstack(td, (uap->uss != NULL) ? &ss : NULL,
	    (uap->uoss != NULL) ? &oss : NULL);
	if (error == 0 && uap->uoss != NULL) {
		lss.ss_sp = PTROUT(oss.ss_sp);
		lss.ss_size = oss.ss_size;
		lss.ss_flags = bsd_to_linux_sigaltstack(oss.ss_flags);
		error = copyout(&lss, uap->uoss, sizeof(lss));
	}

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_signal(struct thread *td, struct linux_signal_args *args)
{
	l_sigaction_t nsa, osa;
	int error;

	nsa.lsa_handler = args->handler;
	nsa.lsa_flags = LINUX_SA_ONESHOT | LINUX_SA_NOMASK;
	LINUX_SIGEMPTYSET(nsa.lsa_mask);

	error = linux_do_sigaction(td, args->sig, &nsa, &osa);
	td->td_retval[0] = (int)(intptr_t)osa.lsa_handler;

	return (error);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_rt_sigaction(struct thread *td, struct linux_rt_sigaction_args *args)
{
	l_sigaction_t nsa, osa;
	int error;

	if (args->sigsetsize != sizeof(l_sigset_t))
		return (EINVAL);

	if (args->act != NULL) {
		error = copyin(args->act, &nsa, sizeof(nsa));
		if (error != 0)
			return (error);
	}

	error = linux_do_sigaction(td, args->sig,
				   args->act ? &nsa : NULL,
				   args->oact ? &osa : NULL);

	if (args->oact != NULL && error == 0)
		error = copyout(&osa, args->oact, sizeof(osa));

	return (error);
}

static int
linux_do_sigprocmask(struct thread *td, int how, sigset_t *new,
		     l_sigset_t *old)
{
	sigset_t omask;
	int error;

	td->td_retval[0] = 0;

	switch (how) {
	case LINUX_SIG_BLOCK:
		how = SIG_BLOCK;
		break;
	case LINUX_SIG_UNBLOCK:
		how = SIG_UNBLOCK;
		break;
	case LINUX_SIG_SETMASK:
		how = SIG_SETMASK;
		break;
	default:
		return (EINVAL);
	}
	error = kern_sigprocmask(td, how, new, &omask, 0);
	if (error == 0 && old != NULL)
		bsd_to_linux_sigset(&omask, old);

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_sigprocmask(struct thread *td, struct linux_sigprocmask_args *args)
{
	l_osigset_t mask;
	l_sigset_t lset, oset;
	sigset_t set;
	int error;

	if (args->mask != NULL) {
		error = copyin(args->mask, &mask, sizeof(mask));
		if (error != 0)
			return (error);
		LINUX_SIGEMPTYSET(lset);
		lset.__mask = mask;
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&lset, sizeof(lset));
#endif
		linux_to_bsd_sigset(&lset, &set);
	}

	error = linux_do_sigprocmask(td, args->how,
				     args->mask ? &set : NULL,
				     args->omask ? &oset : NULL);

	if (args->omask != NULL && error == 0) {
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&oset, sizeof(oset));
#endif
		mask = oset.__mask;
		error = copyout(&mask, args->omask, sizeof(mask));
	}

	return (error);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_rt_sigprocmask(struct thread *td, struct linux_rt_sigprocmask_args *args)
{
	l_sigset_t oset;
	sigset_t set, *pset;
	int error;

	error = linux_copyin_sigset(td, args->mask, args->sigsetsize,
	    &set, &pset);
	if (error != 0)
		return (EINVAL);

	error = linux_do_sigprocmask(td, args->how, pset,
				     args->omask ? &oset : NULL);

	if (args->omask != NULL && error == 0) {
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&oset, sizeof(oset));
#endif
		error = copyout(&oset, args->omask, sizeof(oset));
	}

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_sgetmask(struct thread *td, struct linux_sgetmask_args *args)
{
	struct proc *p = td->td_proc;
	l_sigset_t mask;

	PROC_LOCK(p);
	bsd_to_linux_sigset(&td->td_sigmask, &mask);
	PROC_UNLOCK(p);
	td->td_retval[0] = mask.__mask;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		linux_ktrsigset(&mask, sizeof(mask));
#endif
	return (0);
}

int
linux_ssetmask(struct thread *td, struct linux_ssetmask_args *args)
{
	struct proc *p = td->td_proc;
	l_sigset_t lset;
	sigset_t bset;

	PROC_LOCK(p);
	bsd_to_linux_sigset(&td->td_sigmask, &lset);
	td->td_retval[0] = lset.__mask;
	LINUX_SIGEMPTYSET(lset);
	lset.__mask = args->mask;
	linux_to_bsd_sigset(&lset, &bset);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		linux_ktrsigset(&lset, sizeof(lset));
#endif
	td->td_sigmask = bset;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);
	return (0);
}

int
linux_sigpending(struct thread *td, struct linux_sigpending_args *args)
{
	struct proc *p = td->td_proc;
	sigset_t bset;
	l_sigset_t lset;
	l_osigset_t mask;

	PROC_LOCK(p);
	bset = p->p_siglist;
	SIGSETOR(bset, td->td_siglist);
	SIGSETAND(bset, td->td_sigmask);
	PROC_UNLOCK(p);
	bsd_to_linux_sigset(&bset, &lset);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		linux_ktrsigset(&lset, sizeof(lset));
#endif
	mask = lset.__mask;
	return (copyout(&mask, args->mask, sizeof(mask)));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

/*
 * MPSAFE
 */
int
linux_rt_sigpending(struct thread *td, struct linux_rt_sigpending_args *args)
{
	struct proc *p = td->td_proc;
	sigset_t bset;
	l_sigset_t lset;

	if (args->sigsetsize > sizeof(lset))
		return (EINVAL);
		/* NOT REACHED */

	PROC_LOCK(p);
	bset = p->p_siglist;
	SIGSETOR(bset, td->td_siglist);
	SIGSETAND(bset, td->td_sigmask);
	PROC_UNLOCK(p);
	bsd_to_linux_sigset(&bset, &lset);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		linux_ktrsigset(&lset, sizeof(lset));
#endif
	return (copyout(&lset, args->set, args->sigsetsize));
}

int
linux_rt_sigtimedwait(struct thread *td,
	struct linux_rt_sigtimedwait_args *args)
{
	struct timespec ts, *tsa;
	int error;

	if (args->timeout) {
		error = linux_get_timespec(&ts, args->timeout);
		if (error != 0)
			return (error);
		tsa = &ts;
	} else
		tsa = NULL;

	return (linux_common_rt_sigtimedwait(td, args->mask, tsa,
	    args->ptr, args->sigsetsize));
}

static int
linux_common_rt_sigtimedwait(struct thread *td, l_sigset_t *mask,
    struct timespec *tsa, l_siginfo_t *ptr, l_size_t sigsetsize)
{
	int error, sig;
	sigset_t bset;
	l_siginfo_t lsi;
	ksiginfo_t ksi;

	error = linux_copyin_sigset(td, mask, sigsetsize, &bset, NULL);
	if (error != 0)
		return (error);

	ksiginfo_init(&ksi);
	error = kern_sigtimedwait(td, bset, &ksi, tsa);
	if (error != 0)
		return (error);

	sig = bsd_to_linux_signal(ksi.ksi_signo);

	if (ptr) {
		memset(&lsi, 0, sizeof(lsi));
		siginfo_to_lsiginfo(&ksi.ksi_info, &lsi, sig);
		error = copyout(&lsi, ptr, sizeof(lsi));
	}
	if (error == 0)
		td->td_retval[0] = sig;

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_rt_sigtimedwait_time64(struct thread *td,
	struct linux_rt_sigtimedwait_time64_args *args)
{
	struct timespec ts, *tsa;
	int error;

	if (args->timeout) {
		error = linux_get_timespec64(&ts, args->timeout);
		if (error != 0)
			return (error);
		tsa = &ts;
	} else
		tsa = NULL;

	return (linux_common_rt_sigtimedwait(td, args->mask, tsa,
	    args->ptr, args->sigsetsize));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_kill(struct thread *td, struct linux_kill_args *args)
{
	int sig;

	/*
	 * Allow signal 0 as a means to check for privileges
	 */
	if (!LINUX_SIG_VALID(args->signum) && args->signum != 0)
		return (EINVAL);

	if (args->signum > 0)
		sig = linux_to_bsd_signal(args->signum);
	else
		sig = 0;

	if (args->pid > PID_MAX)
		return (linux_psignal(td, args->pid, sig));
	else
		return (kern_kill(td, args->pid, sig));
}

int
linux_tgkill(struct thread *td, struct linux_tgkill_args *args)
{
	int sig;

	if (args->pid <= 0 || args->tgid <=0)
		return (EINVAL);

	/*
	 * Allow signal 0 as a means to check for privileges
	 */
	if (!LINUX_SIG_VALID(args->sig) && args->sig != 0)
		return (EINVAL);

	if (args->sig > 0)
		sig = linux_to_bsd_signal(args->sig);
	else
		sig = 0;

	return (linux_tdsignal(td, args->pid, args->tgid, sig));
}

/*
 * Deprecated since 2.5.75. Replaced by tgkill().
 */
int
linux_tkill(struct thread *td, struct linux_tkill_args *args)
{
	int sig;

	if (args->tid <= 0)
		return (EINVAL);

	if (!LINUX_SIG_VALID(args->sig))
		return (EINVAL);

	sig = linux_to_bsd_signal(args->sig);

	return (linux_tdsignal(td, args->tid, -1, sig));
}

static int
sigfpe_sicode2lsicode(int si_code)
{

	switch (si_code) {
	case FPE_INTOVF:
		return (LINUX_FPE_INTOVF);
	case FPE_INTDIV:
		return (LINUX_FPE_INTDIV);
	case FPE_FLTIDO:
		return (LINUX_FPE_FLTUNK);
	default:
		return (si_code);
	}
}

static int
sigbus_sicode2lsicode(int si_code)
{

	switch (si_code) {
	case BUS_OOMERR:
		return (LINUX_BUS_MCEERR_AR);
	default:
		return (si_code);
	}
}

static int
sigsegv_sicode2lsicode(int si_code)
{

	switch (si_code) {
	case SEGV_PKUERR:
		return (LINUX_SEGV_PKUERR);
	default:
		return (si_code);
	}
}

static int
sigtrap_sicode2lsicode(int si_code)
{

	switch (si_code) {
	case TRAP_DTRACE:
		return (LINUX_TRAP_TRACE);
	case TRAP_CAP:
		return (LINUX_TRAP_UNK);
	default:
		return (si_code);
	}
}

static void
sicode_to_lsicode(int sig, int si_code, int *lsi_code)
{

	switch (si_code) {
	case SI_USER:
		*lsi_code = LINUX_SI_USER;
		break;
	case SI_KERNEL:
		*lsi_code = LINUX_SI_KERNEL;
		break;
	case SI_QUEUE:
		*lsi_code = LINUX_SI_QUEUE;
		break;
	case SI_TIMER:
		*lsi_code = LINUX_SI_TIMER;
		break;
	case SI_MESGQ:
		*lsi_code = LINUX_SI_MESGQ;
		break;
	case SI_ASYNCIO:
		*lsi_code = LINUX_SI_ASYNCIO;
		break;
	case SI_LWP:
		*lsi_code = LINUX_SI_TKILL;
		break;
	default:
		switch (sig) {
		case LINUX_SIGFPE:
			*lsi_code = sigfpe_sicode2lsicode(si_code);
			break;
		case LINUX_SIGBUS:
			*lsi_code = sigbus_sicode2lsicode(si_code);
			break;
		case LINUX_SIGSEGV:
			*lsi_code = sigsegv_sicode2lsicode(si_code);
			break;
		case LINUX_SIGTRAP:
			*lsi_code = sigtrap_sicode2lsicode(si_code);
			break;
		default:
			*lsi_code = si_code;
			break;
		}
		break;
	}
}

void
siginfo_to_lsiginfo(const siginfo_t *si, l_siginfo_t *lsi, l_int sig)
{

	/* sig already converted */
	lsi->lsi_signo = sig;
	sicode_to_lsicode(sig, si->si_code, &lsi->lsi_code);

	switch (si->si_code) {
	case SI_LWP:
		lsi->lsi_pid = si->si_pid;
		lsi->lsi_uid = si->si_uid;
		break;

	case SI_TIMER:
		lsi->lsi_int = si->si_value.sival_int;
		lsi->lsi_ptr = PTROUT(si->si_value.sival_ptr);
		lsi->lsi_tid = si->si_timerid;
		break;

	case SI_QUEUE:
		lsi->lsi_pid = si->si_pid;
		lsi->lsi_uid = si->si_uid;
		lsi->lsi_ptr = PTROUT(si->si_value.sival_ptr);
		break;

	case SI_ASYNCIO:
		lsi->lsi_int = si->si_value.sival_int;
		lsi->lsi_ptr = PTROUT(si->si_value.sival_ptr);
		break;

	default:
		switch (sig) {
		case LINUX_SIGPOLL:
			/* XXX si_fd? */
			lsi->lsi_band = si->si_band;
			break;

		case LINUX_SIGCHLD:
			lsi->lsi_errno = 0;
			lsi->lsi_pid = si->si_pid;
			lsi->lsi_uid = si->si_uid;

			if (si->si_code == CLD_STOPPED || si->si_code == CLD_KILLED)
				lsi->lsi_status = bsd_to_linux_signal(si->si_status);
			else if (si->si_code == CLD_CONTINUED)
				lsi->lsi_status = bsd_to_linux_signal(SIGCONT);
			else
				lsi->lsi_status = si->si_status;
			break;

		case LINUX_SIGBUS:
		case LINUX_SIGILL:
		case LINUX_SIGFPE:
		case LINUX_SIGSEGV:
			lsi->lsi_addr = PTROUT(si->si_addr);
			break;

		default:
			lsi->lsi_pid = si->si_pid;
			lsi->lsi_uid = si->si_uid;
			if (sig >= LINUX_SIGRTMIN) {
				lsi->lsi_int = si->si_value.sival_int;
				lsi->lsi_ptr = PTROUT(si->si_value.sival_ptr);
			}
			break;
		}
		break;
	}
}

int
lsiginfo_to_siginfo(struct thread *td, const l_siginfo_t *lsi,
    siginfo_t *si, int sig)
{

	switch (lsi->lsi_code) {
	case LINUX_SI_TKILL:
		if (linux_kernver(td) >= LINUX_KERNVER(2,6,39)) {
			linux_msg(td, "SI_TKILL forbidden since 2.6.39");
			return (EPERM);
		}
		si->si_code = SI_LWP;
	case LINUX_SI_QUEUE:
		si->si_code = SI_QUEUE;
		break;
	case LINUX_SI_TIMER:
		si->si_code = SI_TIMER;
		break;
	case LINUX_SI_MESGQ:
		si->si_code = SI_MESGQ;
		break;
	case LINUX_SI_ASYNCIO:
		si->si_code = SI_ASYNCIO;
		break;
	default:
		si->si_code = lsi->lsi_code;
		break;
	}

	si->si_signo = sig;
	si->si_pid = td->td_proc->p_pid;
	si->si_uid = td->td_ucred->cr_ruid;
	si->si_value.sival_ptr = PTRIN(lsi->lsi_value.sival_ptr);
	return (0);
}

int
linux_rt_sigqueueinfo(struct thread *td, struct linux_rt_sigqueueinfo_args *args)
{
	l_siginfo_t linfo;
	ksiginfo_t ksi;
	int error;
	int sig;

	if (!LINUX_SIG_VALID(args->sig))
		return (EINVAL);

	error = copyin(args->info, &linfo, sizeof(linfo));
	if (error != 0)
		return (error);

	if (linfo.lsi_code >= 0)
		/* SI_USER, SI_KERNEL */
		return (EPERM);

	sig = linux_to_bsd_signal(args->sig);
	ksiginfo_init(&ksi);
	error = lsiginfo_to_siginfo(td, &linfo, &ksi.ksi_info, sig);
	if (error != 0)
		return (error);

	return (linux_pksignal(td, args->pid, sig, &ksi));
}

int
linux_rt_tgsigqueueinfo(struct thread *td, struct linux_rt_tgsigqueueinfo_args *args)
{
	l_siginfo_t linfo;
	ksiginfo_t ksi;
	int error;
	int sig;

	if (!LINUX_SIG_VALID(args->sig))
		return (EINVAL);

	error = copyin(args->uinfo, &linfo, sizeof(linfo));
	if (error != 0)
		return (error);

	if (linfo.lsi_code >= 0)
		return (EPERM);

	sig = linux_to_bsd_signal(args->sig);
	ksiginfo_init(&ksi);
	error = lsiginfo_to_siginfo(td, &linfo, &ksi.ksi_info, sig);
	if (error != 0)
		return (error);

	return (linux_tdksignal(td, args->tid, args->tgid, sig, &ksi));
}

int
linux_rt_sigsuspend(struct thread *td, struct linux_rt_sigsuspend_args *uap)
{
	sigset_t sigmask;
	int error;

	error = linux_copyin_sigset(td, uap->newset, uap->sigsetsize,
	    &sigmask, NULL);
	if (error != 0)
		return (error);

	return (kern_sigsuspend(td, sigmask));
}

static int
linux_tdksignal(struct thread *td, lwpid_t tid, int tgid, int sig,
    ksiginfo_t *ksi)
{
	struct thread *tdt;
	struct proc *p;
	int error;

	tdt = linux_tdfind(td, tid, tgid);
	if (tdt == NULL)
		return (ESRCH);

	p = tdt->td_proc;
	AUDIT_ARG_SIGNUM(sig);
	AUDIT_ARG_PID(p->p_pid);
	AUDIT_ARG_PROCESS(p);

	error = p_cansignal(td, p, sig);
	if (error != 0 || sig == 0)
		goto out;

	tdksignal(tdt, sig, ksi);

out:
	PROC_UNLOCK(p);
	return (error);
}

static int
linux_tdsignal(struct thread *td, lwpid_t tid, int tgid, int sig)
{
	ksiginfo_t ksi;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_LWP;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_proc->p_ucred->cr_ruid;
	return (linux_tdksignal(td, tid, tgid, sig, &ksi));
}

static int
linux_pksignal(struct thread *td, int pid, int sig, ksiginfo_t *ksi)
{
	struct thread *tdt;
	struct proc *p;
	int error;

	tdt = linux_tdfind(td, pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	p = tdt->td_proc;
	AUDIT_ARG_SIGNUM(sig);
	AUDIT_ARG_PID(p->p_pid);
	AUDIT_ARG_PROCESS(p);

	error = p_cansignal(td, p, sig);
	if (error != 0 || sig == 0)
		goto out;

	pksignal(p, sig, ksi);

out:
	PROC_UNLOCK(p);
	return (error);
}

static int
linux_psignal(struct thread *td, int pid, int sig)
{
	ksiginfo_t ksi;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_LWP;
	ksi.ksi_pid = td->td_proc->p_pid;
	ksi.ksi_uid = td->td_proc->p_ucred->cr_ruid;
	return (linux_pksignal(td, pid, sig, &ksi));
}

int
linux_copyin_sigset(struct thread *td, l_sigset_t *lset,
    l_size_t sigsetsize, sigset_t *set, sigset_t **pset)
{
	l_sigset_t lmask;
	int error;

	if (sigsetsize != sizeof(l_sigset_t))
		return (EINVAL);
	if (lset != NULL) {
		error = copyin(lset, &lmask, sizeof(lmask));
		if (error != 0)
			return (error);
		linux_to_bsd_sigset(&lmask, set);
		if (pset != NULL)
			*pset = set;
#ifdef KTRACE
		if (KTRPOINT(td, KTR_STRUCT))
			linux_ktrsigset(&lmask, sizeof(lmask));
#endif
	} else if (pset != NULL)
		*pset = NULL;
	return (0);
}
