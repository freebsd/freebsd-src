/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

void
linux_to_bsd_sigset(linux_sigset_t *lss, sigset_t *bss)
{
	int b, l;

	SIGEMPTYSET(*bss);
	bss->__bits[0] = lss->__bits[0] & ~((1U << LINUX_SIGTBLSZ) - 1);
	bss->__bits[1] = lss->__bits[1];
	for (l = 1; l <= LINUX_SIGTBLSZ; l++) {
		if (LINUX_SIGISMEMBER(*lss, l)) {
			b = linux_to_bsd_signal[_SIG_IDX(l)];
			if (b)
				SIGADDSET(*bss, b);
		}
	}
}

static void
bsd_to_linux_sigset(sigset_t *bss, linux_sigset_t *lss)
{
	int b, l;

	LINUX_SIGEMPTYSET(*lss);
	lss->__bits[0] = bss->__bits[0] & ~((1U << LINUX_SIGTBLSZ) - 1);
	lss->__bits[1] = bss->__bits[1];
	for (b = 1; b <= LINUX_SIGTBLSZ; b++) {
		if (SIGISMEMBER(*bss, b)) {
			l = bsd_to_linux_signal[_SIG_IDX(b)];
			if (l)
				LINUX_SIGADDSET(*lss, l);
		}
	}
}

static void
linux_to_bsd_sigaction(linux_sigaction_t *lsa, struct sigaction *bsa)
{

	linux_to_bsd_sigset(&lsa->lsa_mask, &bsa->sa_mask);
	bsa->sa_handler = lsa->lsa_handler;
	bsa->sa_flags = 0;
	if (lsa->lsa_flags & LINUX_SA_NOCLDSTOP)
		bsa->sa_flags |= SA_NOCLDSTOP;
	if (lsa->lsa_flags & LINUX_SA_NOCLDWAIT)
		bsa->sa_flags |= SA_NOCLDWAIT;
	if (lsa->lsa_flags & LINUX_SA_SIGINFO)
		bsa->sa_flags |= SA_SIGINFO;
	if (lsa->lsa_flags & LINUX_SA_ONSTACK)
		bsa->sa_flags |= SA_ONSTACK;
	if (lsa->lsa_flags & LINUX_SA_RESTART)
		bsa->sa_flags |= SA_RESTART;
	if (lsa->lsa_flags & LINUX_SA_ONESHOT)
		bsa->sa_flags |= SA_RESETHAND;
	if (lsa->lsa_flags & LINUX_SA_NOMASK)
		bsa->sa_flags |= SA_NODEFER;
}

static void
bsd_to_linux_sigaction(struct sigaction *bsa, linux_sigaction_t *lsa)
{

	bsd_to_linux_sigset(&bsa->sa_mask, &lsa->lsa_mask);
	lsa->lsa_handler = bsa->sa_handler;
	lsa->lsa_restorer = NULL;	/* unsupported */
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
linux_do_sigaction(struct proc *p, int linux_sig, linux_sigaction_t *linux_nsa,
		   linux_sigaction_t *linux_osa)
{
	struct sigaction *nsa, *osa;
	struct sigaction_args sa_args;
	int error;
	caddr_t sg = stackgap_init();

	if (linux_sig <= 0 || linux_sig > LINUX_NSIG)
		return (EINVAL);

	if (linux_osa != NULL)
		osa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		osa = NULL;

	if (linux_nsa != NULL) {
		nsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		linux_to_bsd_sigaction(linux_nsa, nsa);
	}
	else
		nsa = NULL;

	if (linux_sig <= LINUX_SIGTBLSZ)
		sa_args.sig = linux_to_bsd_signal[_SIG_IDX(linux_sig)];
	else
		sa_args.sig = linux_sig;

	sa_args.act = nsa;
	sa_args.oact = osa;
	error = sigaction(p, &sa_args);
	if (error)
		return (error);

	if (linux_osa != NULL)
		bsd_to_linux_sigaction(osa, linux_osa);

	return (0);
}

int
linux_signal(struct proc *p, struct linux_signal_args *args)
{
	linux_sigaction_t nsa, osa;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): signal(%d, %p)\n",
	       (long)p->p_pid, args->sig, (void *)args->handler);
#endif

	nsa.lsa_handler = args->handler;
	nsa.lsa_flags = LINUX_SA_ONESHOT | LINUX_SA_NOMASK;
	LINUX_SIGEMPTYSET(nsa.lsa_mask);

	error = linux_do_sigaction(p, args->sig, &nsa, &osa);
	p->p_retval[0] = (int)osa.lsa_handler;

	return (error);
}

int
linux_rt_sigaction(struct proc *p, struct linux_rt_sigaction_args *args)
{
	linux_sigaction_t nsa, osa;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): rt_sigaction(%d, %p, %p, %d)\n",
	       (long)p->p_pid, args->sig, (void *)args->act,
	       (void *)args->oact, args->sigsetsize);
#endif

	if (args->sigsetsize != sizeof(linux_sigset_t))
		return (EINVAL);

	if (args->act != NULL) {
		error = copyin(args->act, &nsa, sizeof(linux_sigaction_t));
		if (error)
			return (error);
	}

	error = linux_do_sigaction(p, args->sig,
				   args->act ? &nsa : NULL,
				   args->oact ? &osa : NULL);

	if (args->oact != NULL && !error) {
		error = copyout(&osa, args->oact, sizeof(linux_sigaction_t));
	}

	return (error);
}

static int
linux_do_sigprocmask(struct proc *p, int how, linux_sigset_t *new,
		     linux_sigset_t *old)
{
	int error, s;
	sigset_t mask;

	error = 0;
	p->p_retval[0] = 0;

	if (old != NULL)
		bsd_to_linux_sigset(&p->p_sigmask, old);

	if (new != NULL) {
		linux_to_bsd_sigset(new, &mask);

		s = splhigh();

		switch (how) {
		case LINUX_SIG_BLOCK:
			SIGSETOR(p->p_sigmask, mask);
			SIG_CANTMASK(p->p_sigmask);
			break;
		case LINUX_SIG_UNBLOCK:
			SIGSETNAND(p->p_sigmask, mask);
			break;
		case LINUX_SIG_SETMASK:
			p->p_sigmask = mask;
			SIG_CANTMASK(p->p_sigmask);
			break;
		default:
			error = EINVAL;
			break;
		}

		splx(s);
	}

	return (error);
}

int
linux_sigprocmask(struct proc *p, struct linux_sigprocmask_args *args)
{
	linux_osigset_t mask;
	linux_sigset_t set, oset;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%d): sigprocmask(%d, *, *)\n", p->p_pid, args->how);
#endif

	if (args->mask != NULL) {
		error = copyin(args->mask, &mask, sizeof(linux_osigset_t));
		if (error)
			return (error);
		LINUX_SIGEMPTYSET(set);
		set.__bits[0] = mask;
	}

	error = linux_do_sigprocmask(p, args->how,
				     args->mask ? &set : NULL,
				     args->omask ? &oset : NULL);

	if (args->omask != NULL && !error) {
		mask = oset.__bits[0];
		error = copyout(&mask, args->omask, sizeof(linux_osigset_t));
	}

	return (error);
}

int
linux_rt_sigprocmask(struct proc *p, struct linux_rt_sigprocmask_args *args)
{
	linux_sigset_t set, oset;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): rt_sigprocmask(%d, %p, %p, %d)\n",
	       (long)p->p_pid, args->how, (void *)args->mask,
	       (void *)args->omask, args->sigsetsize);
#endif

	if (args->sigsetsize != sizeof(linux_sigset_t))
		return EINVAL;

	if (args->mask != NULL) {
		error = copyin(args->mask, &set, sizeof(linux_sigset_t));
		if (error)
			return (error);
	}

	error = linux_do_sigprocmask(p, args->how,
				     args->mask ? &set : NULL,
				     args->omask ? &oset : NULL);

	if (args->omask != NULL && !error) {
		error = copyout(&oset, args->omask, sizeof(linux_sigset_t));
	}

	return (error);
}

int
linux_siggetmask(struct proc *p, struct linux_siggetmask_args *args)
{
	linux_sigset_t mask;

#ifdef DEBUG
	printf("Linux-emul(%d): siggetmask()\n", p->p_pid);
#endif

	bsd_to_linux_sigset(&p->p_sigmask, &mask);
	p->p_retval[0] = mask.__bits[0];
	return (0);
}

int
linux_sigsetmask(struct proc *p, struct linux_sigsetmask_args *args)
{
	linux_sigset_t lset;
	sigset_t bset;
	int s;

#ifdef DEBUG
	printf("Linux-emul(%ld): sigsetmask(%08lx)\n",
	       (long)p->p_pid, (unsigned long)args->mask);
#endif

	bsd_to_linux_sigset(&p->p_sigmask, &lset);
	p->p_retval[0] = lset.__bits[0];
	LINUX_SIGEMPTYSET(lset);
	lset.__bits[0] = args->mask;
	linux_to_bsd_sigset(&lset, &bset);
	s = splhigh();
	p->p_sigmask = bset;
	SIG_CANTMASK(p->p_sigmask);
	splx(s);
	return (0);
}

int
linux_sigpending(struct proc *p, struct linux_sigpending_args *args)
{
	sigset_t bset;
	linux_sigset_t lset;
	linux_osigset_t mask;

#ifdef DEBUG
	printf("Linux-emul(%d): sigpending(*)\n", p->p_pid);
#endif

	bset = p->p_siglist;
	SIGSETAND(bset, p->p_sigmask);
	bsd_to_linux_sigset(&bset, &lset);
	mask = lset.__bits[0];
	return (copyout(&mask, args->mask, sizeof(mask)));
}

int
linux_kill(struct proc *p, struct linux_kill_args *args)
{
	struct kill_args /* {
	    int pid;
	    int signum;
	} */ tmp;

#ifdef DEBUG
	printf("Linux-emul(%d): kill(%d, %d)\n",
	       p->p_pid, args->pid, args->signum);
#endif

	/*
	 * Allow signal 0 as a means to check for privileges
	 */
	if (args->signum < 0 || args->signum > LINUX_NSIG)
		return EINVAL;

	if (args->signum > 0 && args->signum <= LINUX_SIGTBLSZ)
		tmp.signum = linux_to_bsd_signal[_SIG_IDX(args->signum)];
	else
		tmp.signum = args->signum;

	tmp.pid = args->pid;
	return (kill(p, &tmp));
}
