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
 *  $Id: linux_signal.c,v 1.2 1995/11/22 07:43:50 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/signal.h>
#include <sys/signalvar.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

#define DONTMASK    (sigmask(SIGKILL)|sigmask(SIGSTOP)|sigmask(SIGCHLD))

static sigset_t
linux_to_bsd_sigmask(linux_sigset_t mask) {
    int i;
    sigset_t new = 0;

    for (i = 1; i <= LINUX_NSIG; i++)
	if (mask & (1 << i-1))
	    new |= (1 << (linux_to_bsd_signal[i]-1));
    return new;
}

static linux_sigset_t
bsd_to_linux_sigmask(sigset_t mask) {
    int i;
    sigset_t new = 0;

    for (i = 1; i <= NSIG; i++)
	if (mask & (1 << i-1))
	    new |= (1 << (bsd_to_linux_signal[i]-1));
    return new;
}

struct linux_sigaction_args {
    int sig;
    linux_sigaction_t *nsa;
    linux_sigaction_t *osa;
};

int
linux_sigaction(struct proc *p, struct linux_sigaction_args *args, int *retval)
{
    linux_sigaction_t linux_sa;
    struct sigaction *nsa = NULL, *osa = NULL, bsd_sa;
    struct sigaction_args /* {
	int signum;
	struct sigaction *nsa;
	struct sigaction *osa;
    } */ sa;
    int error;
    
#ifdef DEBUG
    printf("Linux-emul(%d): sigaction(%d, *, *)\n", p->p_pid, args->sig);
#endif
    if (args->osa)
	osa = (struct sigaction *)ua_alloc_init(sizeof(struct sigaction));

    if (args->nsa) {
	nsa = (struct sigaction *)ua_alloc(sizeof(struct sigaction));
	if (error = copyin(args->nsa, &linux_sa, sizeof(linux_sigaction_t)))
	    return error;
	bsd_sa.sa_mask = linux_to_bsd_sigmask(linux_sa.sa_mask);
	bsd_sa.sa_handler = linux_sa.sa_handler;
	bsd_sa.sa_flags = 0;
	if (linux_sa.sa_flags & LINUX_SA_NOCLDSTOP)
	    bsd_sa.sa_flags |= SA_NOCLDSTOP;
	if (linux_sa.sa_flags & LINUX_SA_ONSTACK)
	    bsd_sa.sa_flags |= SA_ONSTACK;
	if (linux_sa.sa_flags & LINUX_SA_RESTART)
	    bsd_sa.sa_flags |= SA_RESTART;
	if (error = copyout(&bsd_sa, nsa, sizeof(struct sigaction)))
	    return error;
    }
    sa.signum = linux_to_bsd_signal[args->sig];
    sa.nsa = nsa;
    sa.osa = osa;
    if ((error = sigaction(p, &sa, retval)))
	return error;

    if (args->osa) {
	if (error = copyin(osa, &bsd_sa, sizeof(struct sigaction)))
	    return error;
	linux_sa.sa_handler = bsd_sa.sa_handler;
	linux_sa.sa_restorer = NULL;
	linux_sa.sa_mask = bsd_to_linux_sigmask(bsd_sa.sa_mask);
	linux_sa.sa_flags = 0;
	if (bsd_sa.sa_flags & SA_NOCLDSTOP)
	    linux_sa.sa_flags |= LINUX_SA_NOCLDSTOP;
	if (bsd_sa.sa_flags & SA_ONSTACK)
	    linux_sa.sa_flags |= LINUX_SA_ONSTACK;
	if (bsd_sa.sa_flags & SA_RESTART)
	    linux_sa.sa_flags |= LINUX_SA_RESTART;
	if (error = copyout(&linux_sa, args->osa, sizeof(linux_sigaction_t)))
	    return error;
    }
    return 0;
}

struct linux_sigprocmask_args {
    int how;
    linux_sigset_t *mask;
    linux_sigset_t *omask;
};

int
linux_sigprocmask(struct proc *p, struct linux_sigprocmask_args *args,
		  int *retval)
{
    int error, s;
    sigset_t mask;
    sigset_t omask;

#ifdef DEBUG
    printf("Linux-emul(%d): sigprocmask(%d, *, *)\n", p->p_pid, args->how);
#endif
    if (args->omask != NULL) {
	omask = bsd_to_linux_sigmask(p->p_sigmask);
	if (error = copyout(&omask, args->omask, sizeof(sigset_t)))
	    return error;
    }
    if (!(args->mask))
	return 0;
    if (error = copyin(args->mask, &mask, sizeof(linux_sigset_t)))
	return error;

    mask = linux_to_bsd_sigmask(mask);
    s = splhigh();
    switch (args->how) {
    case LINUX_SIG_BLOCK:
	p->p_sigmask |= (mask & ~DONTMASK);
	break;
    case LINUX_SIG_UNBLOCK:
	p->p_sigmask &= ~mask;
	break;
    case LINUX_SIG_SETMASK:
	p->p_sigmask = (mask & ~DONTMASK);
	break;
    default:
	error = EINVAL;
	break;
    }
    splx(s);
    return error;
}

int
linux_siggetmask(struct proc *p, void *args, int *retval)
{
#ifdef DEBUG
    printf("Linux-emul(%d): siggetmask()\n", p->p_pid);
#endif
    *retval = bsd_to_linux_sigmask(p->p_sigmask);
    return 0;
}

struct linux_sigsetmask_args {
    linux_sigset_t mask;
};

int
linux_sigsetmask(struct proc *p, struct linux_sigsetmask_args *args,int *retval)
{
    int s;

#ifdef DEBUG
    printf("Linux-emul(%d): sigsetmask(%08x)\n", p->p_pid, args->mask);
#endif
    s = splhigh();
    p->p_sigmask = (linux_to_bsd_sigmask(args->mask) & ~DONTMASK);
    splx(s);
    *retval = bsd_to_linux_sigmask(p->p_sigmask);
    return 0;
}

struct linux_sigpending_args {
    linux_sigset_t *mask;
};

int
linux_sigpending(struct proc *p, struct linux_sigpending_args *args,int *retval)
{
    linux_sigset_t linux_sig;

#ifdef DEBUG
    printf("Linux-emul(%d): sigpending(*)\n", p->p_pid);
#endif
    linux_sig = bsd_to_linux_sigmask(p->p_siglist & p->p_sigmask);
    return copyout(&linux_sig, args->mask, sizeof(linux_sig));
}

struct linux_sigsuspend_args {
    linux_sigset_t mask;
};

int
linux_sigsuspend(struct proc *p, struct linux_sigsuspend_args *args,int *retval)
{
    struct sigsuspend_args /* {
	int mask;
    } */ tmp;

#ifdef DEBUG
    printf("Linux-emul(%d): sigsuspend(%08x)\n", p->p_pid, args->mask);
#endif
    tmp.mask = linux_to_bsd_sigmask(args->mask);
    return sigsuspend(p, &tmp , retval);
}

struct linux_kill_args {
    int pid;
    int signum;
};

int
linux_kill(struct proc *p, struct linux_kill_args *args, int *retval)
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
    return kill(p, &tmp, retval);
}
