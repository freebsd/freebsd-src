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

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <i386/linux/linux_util.h>

static sigset_t
linux_to_bsd_sigset(linux_sigset_t mask) {
    int b, l;
    sigset_t new = 0;

    for (l = 1; l < LINUX_NSIG; l++) {
	if (mask & (1 << (l - 1))) {
	    if ((b = linux_to_bsd_signal[l]))
		new |= (1 << (b - 1));
	}
    }
    return new;
}

static linux_sigset_t
bsd_to_linux_sigset(sigset_t mask) {
    int b, l;
    sigset_t new = 0;

    for (b = 1; b < NSIG; b++) {
	if (mask & (1 << (b - 1))) {
	    if ((l = bsd_to_linux_signal[b]))
		new |= (1 << (l - 1));
	}
    }
    return new;
}

static void
linux_to_bsd_sigaction(linux_sigaction_t *lsa, struct sigaction *bsa)
{
    bsa->sa_mask = linux_to_bsd_sigset(lsa->sa_mask);
    bsa->sa_handler = lsa->sa_handler;
    bsa->sa_flags = 0;
    if (lsa->sa_flags & LINUX_SA_NOCLDSTOP)
	bsa->sa_flags |= SA_NOCLDSTOP;
    if (lsa->sa_flags & LINUX_SA_NOCLDWAIT)
	bsa->sa_flags |= SA_NOCLDWAIT;
    if (lsa->sa_flags & LINUX_SA_ONSTACK)
	bsa->sa_flags |= SA_ONSTACK;
    if (lsa->sa_flags & LINUX_SA_RESTART)
	bsa->sa_flags |= SA_RESTART;
    if (lsa->sa_flags & LINUX_SA_ONESHOT)
	bsa->sa_flags |= SA_RESETHAND;
    if (lsa->sa_flags & LINUX_SA_NOMASK)
	bsa->sa_flags |= SA_NODEFER;
}

static void
bsd_to_linux_sigaction(struct sigaction *bsa, linux_sigaction_t *lsa)
{
    lsa->sa_handler = bsa->sa_handler;
    lsa->sa_restorer = NULL;	/* unsupported */
    lsa->sa_mask = bsd_to_linux_sigset(bsa->sa_mask);
    lsa->sa_flags = 0;
    if (bsa->sa_flags & SA_NOCLDSTOP)
	lsa->sa_flags |= LINUX_SA_NOCLDSTOP;
    if (bsa->sa_flags & SA_NOCLDWAIT)
	lsa->sa_flags |= LINUX_SA_NOCLDWAIT;
    if (bsa->sa_flags & SA_ONSTACK)
	lsa->sa_flags |= LINUX_SA_ONSTACK;
    if (bsa->sa_flags & SA_RESTART)
	lsa->sa_flags |= LINUX_SA_RESTART;
    if (bsa->sa_flags & SA_RESETHAND)
	lsa->sa_flags |= LINUX_SA_ONESHOT;
    if (bsa->sa_flags & SA_NODEFER)
	lsa->sa_flags |= LINUX_SA_NOMASK;
}

static int
linux_do_sigaction(struct proc *p, int linux_sig, linux_sigaction_t *linux_nsa,
                   linux_sigaction_t *linux_osa)
{
	struct sigaction *nsa, *osa, sa;
	struct sigaction_args sa_args;
	int error;
	caddr_t sg = stackgap_init();

	if (linux_sig <= 0 || linux_sig >= LINUX_NSIG)
		return EINVAL;

	if (linux_osa)
		osa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		osa = NULL;

	if (linux_nsa) {
		nsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		linux_to_bsd_sigaction(linux_nsa, &sa);
		error = copyout(&sa, nsa, sizeof(struct sigaction));
		if (error)
			return error;
	}
	else
		nsa = NULL;

	sa_args.signum = linux_to_bsd_signal[linux_sig];
	sa_args.nsa = nsa;
	sa_args.osa = osa;
	error = sigaction(p, &sa_args);
	if (error)
		return error;

	if (linux_osa) {
		error = copyin(osa, &sa, sizeof(struct sigaction));
		if (error)
			return error;
		bsd_to_linux_sigaction(&sa, linux_osa);
	}

	return 0;
}

int
linux_sigaction(struct proc *p, struct linux_sigaction_args *args)
{
	linux_sigaction_t nsa, osa;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): sigaction(%d, %p, %p)\n", (long)p->p_pid,
	       args->sig, (void *)args->nsa, (void *)args->osa);
#endif

	if (args->nsa) {
		error = copyin(args->nsa, &nsa, sizeof(linux_sigaction_t));
		if (error)
			return error;
	}

	error = linux_do_sigaction(p, args->sig,
				   args->nsa ? &nsa : NULL,
				   args->osa ? &osa : NULL);
	if (error)
		return error;

	if (args->osa) {
		error = copyout(&osa, args->osa, sizeof(linux_sigaction_t));
		if (error)
			return error;
	}

	return 0;
}

int
linux_signal(struct proc *p, struct linux_signal_args *args)
{
	linux_sigaction_t nsa, osa;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): signal(%d, %p)\n", (long)p->p_pid,
	       args->sig, (void *)args->handler);
#endif

	nsa.sa_handler = args->handler;
	nsa.sa_flags = LINUX_SA_ONESHOT | LINUX_SA_NOMASK;
	nsa.sa_mask = NULL;

	error = linux_do_sigaction(p, args->sig, &nsa, &osa);

	p->p_retval[0] = (int)osa.sa_handler;

	return 0;
}

int
linux_rt_sigaction(struct proc *p, struct linux_rt_sigaction_args *args)
{
	linux_sigaction_t nsa, osa;
	linux_new_sigaction_t new_sa;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): rt_sigaction(%d, %p, %p, %d)\n",
	       (long)p->p_pid, args->sig, (void *)args->act,
	       (void *)args->oact, args->sigsetsize);
#endif

	if (args->sigsetsize != sizeof(linux_new_sigset_t))
		return EINVAL;

#ifdef DEBUG
	if (args->sig >= LINUX_NSIG) {
		printf("LINUX(%ld): rt_sigaction: 64-bit signal (%d)\n",
		       (long)p->p_pid, args->sig);
	}
#endif

	if (args->act) {
		error = copyin(args->act, &new_sa, sizeof(linux_new_sigaction_t));
		if (error)
			return error;

		nsa.sa_handler = new_sa.lsa_handler;
		nsa.sa_mask = new_sa.lsa_mask.sig[0];
		nsa.sa_flags = new_sa.lsa_flags;
		nsa.sa_restorer = new_sa.lsa_restorer;

#ifdef DEBUG
		if (new_sa.lsa_mask.sig[1] != 0)
			printf("LINUX(%ld): rt_sigaction: sig[1] = 0x%08lx\n",
			       (long)p->p_pid, new_sa.lsa_mask.sig[1]);
#endif
	}

	error = linux_do_sigaction(p, args->sig,
				   args->act ? &nsa : NULL,
				   args->oact ? &osa : NULL);
	if (error)
		return error;

	if (args->oact) {
		new_sa.lsa_handler = osa.sa_handler;
		new_sa.lsa_flags = osa.sa_flags;
		new_sa.lsa_restorer = osa.sa_restorer;
		new_sa.lsa_mask.sig[0] = osa.sa_mask;
		new_sa.lsa_mask.sig[1] = 0;
		error = copyout(&osa, args->oact, sizeof(linux_new_sigaction_t));
		if (error)
			return error;
	}

	return 0;
}

static int
linux_do_sigprocmask(struct proc *p, int how, linux_sigset_t *new,
                     linux_sigset_t *old)
{
	int error = 0, s;
	sigset_t mask;

	p->p_retval[0] = 0;

	if (old != NULL)
		*old = bsd_to_linux_sigset(p->p_sigmask);

	if (new != NULL) {
		mask = linux_to_bsd_sigset(*new);

		s = splhigh();

		switch (how) {
		case LINUX_SIG_BLOCK:
			p->p_sigmask |= (mask & ~sigcantmask);
			break;
		case LINUX_SIG_UNBLOCK:
			p->p_sigmask &= ~mask;
			break;
		case LINUX_SIG_SETMASK:
			p->p_sigmask = (mask & ~sigcantmask);
			break;
		default:
			error = EINVAL;
			break;
		}

		splx(s);
	}

	return error;
}

int
linux_sigprocmask(struct proc *p, struct linux_sigprocmask_args *args)
{
	linux_sigset_t mask;
	linux_sigset_t omask;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): sigprocmask(%d, *, *)\n", (long)p->p_pid,
	       args->how);
#endif

	if (args->mask != NULL) {
		error = copyin(args->mask, &mask, sizeof(linux_sigset_t));
		if (error)
			return error;
	}

	error = linux_do_sigprocmask(p, args->how,
				     args->mask ? &mask : NULL,
				     args->omask ? &omask : NULL);

	if (!error && args->omask != NULL) {
		error = copyout(&omask, args->omask, sizeof(linux_sigset_t));
	}

	return error;
}

int
linux_rt_sigprocmask(struct proc *p, struct linux_rt_sigprocmask_args *args)
{
	linux_new_sigset_t new_mask;
	linux_sigset_t old_mask;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): rt_sigprocmask(%d, %p, %p, %d)\n",
	       (long)p->p_pid, args->how, (void *)args->mask,
	       (void *)args->omask, args->sigsetsize);
#endif

	if (args->sigsetsize != sizeof(linux_new_sigset_t))
		return EINVAL;

	if (args->mask != NULL) {
		error = copyin(args->mask, &new_mask, sizeof(linux_new_sigset_t));
		if (error)
			return error;

#ifdef DEBUG
		if (new_mask.sig[1] != 0)
			printf("LINUX(%ld): rt_sigprocmask: sig[1] = 0x%08lx\n",
			       (long)p->p_pid, new_mask.sig[1]);
#endif
	}

	error = linux_do_sigprocmask(p, args->how,
				     args->mask ? new_mask.sig : NULL,
				     args->omask ? &old_mask : NULL);

	if (!error && args->omask != NULL) {
		new_mask.sig[0] = old_mask;
		error = copyout(&new_mask, args->omask, sizeof(linux_new_sigset_t));
	}

	return error;
}

int
linux_siggetmask(struct proc *p, struct linux_siggetmask_args *args)
{
#ifdef DEBUG
    printf("Linux-emul(%d): siggetmask()\n", p->p_pid);
#endif
    p->p_retval[0] = bsd_to_linux_sigset(p->p_sigmask);
    return 0;
}

int
linux_sigsetmask(struct proc *p, struct linux_sigsetmask_args *args)
{
    int s;
    sigset_t mask;

#ifdef DEBUG
    printf("Linux-emul(%ld): sigsetmask(%08lx)\n",
	(long)p->p_pid, (unsigned long)args->mask);
#endif
    p->p_retval[0] = bsd_to_linux_sigset(p->p_sigmask);

    mask = linux_to_bsd_sigset(args->mask);
    s = splhigh();
    p->p_sigmask = mask & ~sigcantmask;
    splx(s);
    return 0;
}

int
linux_sigpending(struct proc *p, struct linux_sigpending_args *args)
{
    linux_sigset_t linux_sig;

#ifdef DEBUG
    printf("Linux-emul(%d): sigpending(*)\n", p->p_pid);
#endif
    linux_sig = bsd_to_linux_sigset(p->p_siglist & p->p_sigmask);
    return copyout(&linux_sig, args->mask, sizeof(linux_sig));
}

/*
 * Linux has two extra args, restart and oldmask.  We dont use these,
 * but it seems that "restart" is actually a context pointer that
 * enables the signal to happen with a different register set.
 */
int
linux_sigsuspend(struct proc *p, struct linux_sigsuspend_args *args)
{
    struct sigsuspend_args tmp;

#ifdef DEBUG
    printf("Linux-emul(%ld): sigsuspend(%08lx)\n",
	(long)p->p_pid, (unsigned long)args->mask);
#endif
    tmp.mask = linux_to_bsd_sigset(args->mask);
    return sigsuspend(p, &tmp);
}

int
linux_pause(struct proc *p, struct linux_pause_args *args)
{
    struct sigsuspend_args tmp;

#ifdef DEBUG
    printf("Linux-emul(%d): pause()\n", p->p_pid);
#endif
    tmp.mask = p->p_sigmask;
    return sigsuspend(p, &tmp);
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
    if (args->signum < 0 || args->signum >= LINUX_NSIG)
	return EINVAL;
    tmp.pid = args->pid;
    tmp.signum = linux_to_bsd_signal[args->signum];
    return kill(p, &tmp);
}
