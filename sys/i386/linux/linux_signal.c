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
 *  $Id: linux_signal.c,v 1.9 1997/07/20 16:06:03 bde Exp $
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

    for (l = 1; l <= LINUX_NSIG; l++) {
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

    for (b = 1; b <= NSIG; b++) {
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
    if (bsa->sa_flags & SA_ONSTACK)
	lsa->sa_flags |= LINUX_SA_ONSTACK;
    if (bsa->sa_flags & SA_RESTART)
	lsa->sa_flags |= LINUX_SA_RESTART;
    if (bsa->sa_flags & SA_RESETHAND)
	lsa->sa_flags |= LINUX_SA_ONESHOT;
    if (bsa->sa_flags & SA_NODEFER)
	lsa->sa_flags |= LINUX_SA_NOMASK;
}

int
linux_sigaction(struct proc *p, struct linux_sigaction_args *args)
{
    linux_sigaction_t linux_sa;
    struct sigaction *nsa = NULL, *osa = NULL, bsd_sa;
    struct sigaction_args sa;
    int error;
    caddr_t sg = stackgap_init();
    
#ifdef DEBUG
    printf("Linux-emul(%d): sigaction(%d, %08x, %08x)\n", p->p_pid, args->sig,
	args->nsa, args->osa);
#endif

    if (args->osa)
	osa = (struct sigaction *)stackgap_alloc(&sg, sizeof(struct sigaction));

    if (args->nsa) {
	nsa = (struct sigaction *)stackgap_alloc(&sg, sizeof(struct sigaction));
	if (error = copyin(args->nsa, &linux_sa, sizeof(linux_sigaction_t)))
	    return error;
	linux_to_bsd_sigaction(&linux_sa, &bsd_sa);
	if (error = copyout(&bsd_sa, nsa, sizeof(struct sigaction)))
	    return error;
    }
    sa.signum = linux_to_bsd_signal[args->sig];
    sa.nsa = nsa;
    sa.osa = osa;
    if ((error = sigaction(p, &sa)))
	return error;

    if (args->osa) {
	if (error = copyin(osa, &bsd_sa, sizeof(struct sigaction)))
	    return error;
	bsd_to_linux_sigaction(&bsd_sa, &linux_sa);
	if (error = copyout(&linux_sa, args->osa, sizeof(linux_sigaction_t)))
	    return error;
    }
    return 0;
}

int
linux_signal(struct proc *p, struct linux_signal_args *args)
{
    caddr_t sg;
    struct sigaction_args sa_args;
    struct sigaction *osa, *nsa, tmpsa;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): signal(%d, %08x)\n", p->p_pid,
	    args->sig, args->handler);
#endif
    sg = stackgap_init();
    nsa = stackgap_alloc(&sg, sizeof *nsa);
    osa = stackgap_alloc(&sg, sizeof *osa);

    tmpsa.sa_handler = args->handler;
    tmpsa.sa_mask = (sigset_t) 0;
    tmpsa.sa_flags = SA_RESETHAND | SA_NODEFER;
    if ((error = copyout(&tmpsa, nsa, sizeof tmpsa)))
	return error;

    sa_args.signum = linux_to_bsd_signal[args->sig];
    sa_args.osa = osa;
    sa_args.nsa = nsa;
    if ((error = sigaction(p, &sa_args)))
	return error;

    if ((error = copyin(osa, &tmpsa, sizeof *osa)))
	return error;

    p->p_retval[0] = (int)tmpsa.sa_handler;

    return 0;
}


int
linux_sigprocmask(struct proc *p, struct linux_sigprocmask_args *args)
{
    int error, s;
    sigset_t mask;
    sigset_t omask;

#ifdef DEBUG
    printf("Linux-emul(%d): sigprocmask(%d, *, *)\n", p->p_pid, args->how);
#endif

    p->p_retval[0] = 0;

    if (args->omask != NULL) {
	omask = bsd_to_linux_sigset(p->p_sigmask);
	if (error = copyout(&omask, args->omask, sizeof(sigset_t)))
	    return error;
    }
    if (!(args->mask))
	return 0;
    if (error = copyin(args->mask, &mask, sizeof(linux_sigset_t)))
	return error;

    mask = linux_to_bsd_sigset(mask);
    s = splhigh();
    switch (args->how) {
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
    printf("Linux-emul(%d): sigsetmask(%08x)\n", p->p_pid, args->mask);
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
    printf("Linux-emul(%d): sigsuspend(%08x)\n", p->p_pid, args->mask);
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
    tmp.pid = args->pid;
    tmp.signum = linux_to_bsd_signal[args->signum];
    return kill(p, &tmp);
}
