/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
 * All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	$Id: ibcs2_signal.c,v 1.9 1994/10/12 19:38:03 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/vnode.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>


#define DONTMASK	(sigmask(SIGKILL)|sigmask(SIGSTOP)|sigmask(SIGCHLD))

int bsd_to_ibcs2_signal[NSIG] = {
  	0, IBCS2_SIGHUP, IBCS2_SIGINT, IBCS2_SIGQUIT,
  	IBCS2_SIGILL, IBCS2_SIGTRAP, IBCS2_SIGABRT, IBCS2_SIGEMT,
  	IBCS2_SIGFPE, IBCS2_SIGKILL, IBCS2_SIGBUS, IBCS2_SIGSEGV, 
  	IBCS2_SIGSYS, IBCS2_SIGPIPE, IBCS2_SIGALRM, IBCS2_SIGTERM,
  	IBCS2_SIGURG, IBCS2_SIGSTOP, IBCS2_SIGTSTP, IBCS2_SIGCONT,  
  	IBCS2_SIGCHLD, IBCS2_SIGTTIN, IBCS2_SIGTTOU, IBCS2_SIGIO, 
  	IBCS2_SIGGXCPU, IBCS2_SIGGXFSZ, IBCS2_SIGVTALRM, IBCS2_SIGPROF, 
  	IBCS2_SIGWINCH, 0, IBCS2_SIGUSR1, IBCS2_SIGUSR2
};

int ibcs2_to_bsd_signal[IBCS2_NSIG] = {
  	0, SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGEMT,
  	SIGFPE, SIGKILL, SIGBUS, SIGSEGV, SIGSYS, SIGPIPE, SIGALRM, SIGTERM, 
  	SIGUSR1, SIGUSR2, SIGCHLD, 0, SIGWINCH, SIGURG, SIGIO, SIGSTOP, 
  	SIGTSTP, SIGCONT, SIGTTIN, SIGTTOU, SIGVTALRM, SIGPROF, SIGXCPU, SIGXFSZ
};

static char ibcs2_sig_name[IBCS2_NSIG][10] = {
  	"UNKNOWN", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", 
	"SIGABRT", "SIGEMT", "SIGFPE", "SIGKILL", "SIGBUS", "SIGSEGV",
	"SIGSYS", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1", "SIGUSR2",
	"SIGCHLD", "SIGPWR", "SIGWINCH", "SIGURG", "SIGIO", "SIGSTOP", 
	"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM", 
	"SIGPROF", "SIGXCPU", "SIGXFSZ"
};

/*#define LEGAL_SIG(x)	\
	(((x & IBCS2_SIGMASK) < IBCS2_NSIG) ? (x & IBCS2_SIGMASK) : (0))*/
int
LEGAL_SIG(int sig)
{
	if ((sig & IBCS2_SIGMASK) > IBCS2_NSIG) {
		printf("IBCS2: illegal ibcs2 signal %d(%08x)\n", 
			sig & IBCS2_SIGMASK, sig);
		return 0;
	}
	else
		return (sig & IBCS2_SIGMASK);
}

char *
ibcs2_sig_to_str(int sig)
{
	if (sig > IBCS2_NSIG) {
		printf("IBCS2: ibcs2 signal out of range (%d)\n", sig);
		return ibcs2_sig_name[0];
	}
	else
		return ibcs2_sig_name[sig];
}

static sig_t
ibcs2_to_bsd_sigfunc(ibcs2_sig_t func) {
	switch ((int)func) {
	case IBCS2_SIG_DFL:
		return SIG_DFL;
	case IBCS2_SIG_IGN:
		return SIG_IGN;
	case IBCS2_SIG_HOLD:
		return SIG_HOLD;
	default:
		return func;
	}
}

static ibcs2_sig_t
bsd_to_ibcs2_sigfunc(sig_t func) {
	switch ((int)func) {
	case SIG_DFL:
		return IBCS2_SIG_DFL;
	case SIG_IGN:
		return IBCS2_SIG_IGN;
	case SIG_CATCH:
		printf("IBCS2: Oops - SIG_CATCH does not translate :-(\n");
		return IBCS2_SIG_DFL;
	case SIG_HOLD:
		return IBCS2_SIG_HOLD;
	default:
		return func;
	}
}

static sigset_t
ibcs2_to_bsd_sigmask(ibcs2_sigset_t mask) {
  	int i;
  	sigset_t new = 0;

  	for (i = 1; i < NSIG; i++)
    		if (mask & (1 << i-1))
      			new |= (1 << (ibcs2_to_bsd_signal[i]-1));
  	return new;
}

static ibcs2_sigset_t
bsd_to_ibcs2_sigmask(sigset_t mask) {
  	int i;
  	sigset_t new = 0;

  	for (i = 1; i < IBCS2_NSIG; i++)
    		if (mask & (1 << i-1))
      			new |= (1 << (bsd_to_ibcs2_signal[i]-1));
  	return new;
}

struct ibcs2_signal_args {
	int signo;
	ibcs2_sig_t func;
};

static int
ibcs2_sigset(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	struct sigaction tmp;
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

    	*retval = (int)bsd_to_ibcs2_sigfunc(p->p_sigacts->ps_sigact[sig_bsd]);
	if (args->func == IBCS2_SIG_HOLD) {
    		(void) splhigh();
    		p->p_sigmask |= (sigmask(sig_bsd) &~ DONTMASK);
    		(void) spl0();
	}
	else {
		tmp.sa_mask = sigmask(sig_bsd);
    		tmp.sa_handler = ibcs2_to_bsd_sigfunc(args->func);
  		tmp.sa_flags = 0;
   		setsigvec(p, sig_bsd, &tmp);
	}
    	return 0;
} 

static int
ibcs2_sighold(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

	(void) splhigh();
    	*retval = p->p_sigmask;
    	p->p_sigmask |= (sigmask(sig_bsd) & ~DONTMASK);
    	(void) spl0();
    	return 0;
} 

static int
ibcs2_sigrelse(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

	(void) splhigh();
    	*retval = p->p_sigmask;
    	p->p_sigmask &= ~sigmask(sig_bsd);
    	(void) spl0();
    	return 0;
}

static int
ibcs2_sigignore(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	struct sigaction tmp;
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

	tmp.sa_mask = sigmask(sig_bsd);
    	tmp.sa_handler = SIG_IGN;
    	tmp.sa_flags = 0;
    	*retval = (int)bsd_to_ibcs2_sigfunc(p->p_sigacts->ps_sigact[sig_bsd]);
	setsigvec(p, sig_bsd, &tmp);
    	return 0;
}

static int
ibcs2_sigpause(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	struct sigacts *ps = p->p_sigacts;
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SAS_OLDMASK;
	p->p_sigmask = sigmask(sig_bsd) &~ DONTMASK;
	(void) tsleep((caddr_t) ps, PPAUSE|PCATCH, "i-pause", 0);
  	*retval = -1;
    	return EINTR;
} 

static int
ibcs2_signal(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	struct sigaction tmp;
	int sig_bsd = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];

    	tmp.sa_mask = sigmask(sig_bsd);
    	tmp.sa_handler = ibcs2_to_bsd_sigfunc(args->func);
    	tmp.sa_flags = 0;
    	*retval = (int)bsd_to_ibcs2_sigfunc(p->p_sigacts->ps_sigact[sig_bsd]);
	setsigvec(p, sig_bsd, &tmp);
    	return 0;
}

int
ibcs2_sigsys(struct proc *p, struct ibcs2_signal_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
		printf("IBCS2: 'sigsys' signo=%d(%s) ", 
			args->signo & IBCS2_SIGMASK, 
			ibcs2_sig_to_str(args->signo & IBCS2_SIGMASK));

	switch (args->signo & ~IBCS2_SIGMASK ) {
	case 0x0000:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("signal() func=%x\n", args->func);
		return ibcs2_signal(p, args, retval);
	case 0x0100:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("sigset() func=%x\n", args->func);
		return ibcs2_sigset(p, args, retval);
	case 0x0200:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("sighold()\n");
		return ibcs2_sighold(p, args, retval);
	case 0x0400:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("sigrelse()\n");
		return ibcs2_sigrelse(p, args, retval);
	case 0x0800:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("sigignore()\n");
		return ibcs2_sigignore(p, args, retval);
	case 0x1000:
		if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
			printf("sigpause()\n");
		return ibcs2_sigpause(p, args, retval);
	default:
		printf("IBCS2: unknown signal action\n"); break;
	}
	*retval = -1;
  	return EINVAL;
}

struct ibcs2_sigaction_args {
  	int signo;
  	struct sigaction *osa, *nsa;
};

int
ibcs2_sigaction(struct proc *p, struct ibcs2_sigaction_args *args, int *retval)
{
	struct sigaction vec;
	register struct sigaction *sa;
	register struct sigacts *ps = p->p_sigacts;
	register int sig;
	int bit, error;

	if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
		printf("IBCS2: 'sigaction' signo=%d(%s)\n", 
			args->signo, ibcs2_sig_to_str(args->signo));
	sig = ibcs2_to_bsd_signal[LEGAL_SIG(args->signo)];
	if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP) {
		*retval = -1;
		return EINVAL;
	}
	sa = &vec;
	if (args->osa) {
		sa->sa_handler = ps->ps_sigact[sig];
		sa->sa_mask = ps->ps_catchmask[sig];
		bit = sigmask(sig);
		sa->sa_flags = 0;
		if (p->p_flag & SA_NOCLDSTOP)
			sa->sa_flags = IBCS2_SA_NOCLDSTOP;
		if (error = copyout((caddr_t)sa, (caddr_t)args->osa,
		    sizeof(vec))) {
			*retval = -1;
			return error;
		}
	}
	if (args->nsa) {
		if (error = copyin((caddr_t)args->nsa, (caddr_t)sa,
		    sizeof(vec))) {
			*retval = -1;
			return error;
		}
		/*
		 * iBCS2 only defines one SA_ flag right now
		 */
		if (vec.sa_flags & IBCS2_SA_NOCLDSTOP)
		  vec.sa_flags = SA_NOCLDSTOP;
		setsigvec(p, sig, sa);
	}
	*retval = 0;
	return 0;
}

struct ibcs2_sigprocmask_args {
  	int how;
  	unsigned long *mask;
  	unsigned long *omask;
};

int
ibcs2_sigprocmask(struct proc *p, struct ibcs2_sigprocmask_args *args, int *retval)
{
	int error;
	sigset_t umask;
	sigset_t omask = bsd_to_ibcs2_sigmask(p->p_sigmask);

	if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
		printf("IBCS2: 'sigprocmask' how=%d\n", args->how);
	if (error = copyin(args->mask, &umask, sizeof(args->mask))) {
		*retval = -1;
	  	return error;
	}
	umask = ibcs2_to_bsd_sigmask(umask);
	if (args->omask) 
	  	if (error = copyout(&omask, args->omask, sizeof(args->omask))) {
			*retval = -1;
	    		return error;
		}
	(void) splhigh();
	switch (args->how) {
	case 0:	/* SIG_SETMASK */
		p->p_sigmask = umask &~ DONTMASK;
		break;
	
	case 1:	/* SIG_BLOCK */
		p->p_sigmask |= (umask &~ DONTMASK);
		break;

	case 2:	/* SIG_UNBLOCK */
		p->p_sigmask &= ~umask;
		break;

	default:
		error = EINVAL;
		break;
	}
	(void) spl0();
	if (error)
		*retval = -1;
	else
		*retval = 0;
 	return error;
}

struct ibcs2_sigpending_args {
  	unsigned long *sigs;
};

int
ibcs2_sigpending(struct proc *p, struct ibcs2_sigpending_args *args, int *retval)
{
  	int error;
  	sigset_t mask = bsd_to_ibcs2_sigmask(p->p_siglist);
	
	if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
		printf("IBCS2: 'sigpending' which=%x\n", args->sigs);
  	if (error = copyout(&mask, args->sigs, sizeof(unsigned long)))
  		*retval = -1;
	else
		*retval = 0;
    	return error;
}

struct ibcs2_sigsuspend_args {
  	sigset_t *mask;
};

int
ibcs2_sigsuspend(struct proc *p, struct ibcs2_sigsuspend_args *args, int *retval)
{
  	sigset_t mask = ibcs2_to_bsd_sigmask((sigset_t)args->mask);

	if (ibcs2_trace & IBCS2_TRACE_SIGNAL) 
		printf("IBCS2: 'sigsuspend'\n");
	return sigsuspend(p, &mask, retval);
}

struct kill_args {
  	int pid;
  	int signo;
};

int
ibcs2_kill(struct proc *p, struct kill_args *args, int *retval)
{
  	struct kill_args tmp;

	if (ibcs2_trace & IBCS2_TRACE_SIGNAL)
		printf("IBCS2: 'kill' pid=%d, sig=%d(%s)\n", args->pid, 
			args->signo, ibcs2_sig_to_str(args->signo));
  	tmp.pid = args->pid;
  	tmp.signo = (args->signo < IBCS2_NSIG) ? 
			ibcs2_to_bsd_signal[args->signo] : IBCS2_NSIG;
	return kill(p, &tmp, retval); 
}

