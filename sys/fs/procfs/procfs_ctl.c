/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_ctl.c	8.4 (Berkeley) 6/15/94
 *
 * From:
 *	$Id: procfs_ctl.c,v 3.2 1993/12/15 09:40:17 jsp Exp $
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sbuf.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/uio.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

#include <vm/vm.h>

/*
 * True iff process (p) is in trace wait state
 * relative to process (curp)
 */
#define TRACE_WAIT_P(curp, p) \
	 (P_SHOULDSTOP(p) && \
	 (p)->p_pptr == (curp) && \
	 ((p)->p_flag & P_TRACED))

#define PROCFS_CTL_ATTACH	1
#define PROCFS_CTL_DETACH	2
#define PROCFS_CTL_STEP		3
#define PROCFS_CTL_RUN		4
#define PROCFS_CTL_WAIT		5

struct namemap {
	const char *nm_name;
	int nm_val;
};

static struct namemap ctlnames[] = {
	/* special /proc commands */
	{ "attach",	PROCFS_CTL_ATTACH },
	{ "detach",	PROCFS_CTL_DETACH },
	{ "step",	PROCFS_CTL_STEP },
	{ "run",	PROCFS_CTL_RUN },
	{ "wait",	PROCFS_CTL_WAIT },
	{ 0 },
};

static struct namemap signames[] = {
	/* regular signal names */
	{ "hup",	SIGHUP },	{ "int",	SIGINT },
	{ "quit",	SIGQUIT },	{ "ill",	SIGILL },
	{ "trap",	SIGTRAP },	{ "abrt",	SIGABRT },
	{ "iot",	SIGIOT },	{ "emt",	SIGEMT },
	{ "fpe",	SIGFPE },	{ "kill",	SIGKILL },
	{ "bus",	SIGBUS },	{ "segv",	SIGSEGV },
	{ "sys",	SIGSYS },	{ "pipe",	SIGPIPE },
	{ "alrm",	SIGALRM },	{ "term",	SIGTERM },
	{ "urg",	SIGURG },	{ "stop",	SIGSTOP },
	{ "tstp",	SIGTSTP },	{ "cont",	SIGCONT },
	{ "chld",	SIGCHLD },	{ "ttin",	SIGTTIN },
	{ "ttou",	SIGTTOU },	{ "io",		SIGIO },
	{ "xcpu",	SIGXCPU },	{ "xfsz",	SIGXFSZ },
	{ "vtalrm",	SIGVTALRM },	{ "prof",	SIGPROF },
	{ "winch",	SIGWINCH },	{ "info",	SIGINFO },
	{ "usr1",	SIGUSR1 },	{ "usr2",	SIGUSR2 },
	{ 0 },
};

static int	procfs_control(struct thread *td, struct proc *p, int op);

static int
procfs_control(struct thread *td, struct proc *p, int op)
{
	int error = 0;

	/*
	 * Attach - attaches the target process for debugging
	 * by the calling process.
	 */
	if (op == PROCFS_CTL_ATTACH) {
		sx_xlock(&proctree_lock);
		PROC_LOCK(p);
		if ((error = p_candebug(td, p)) != 0)
			goto out;
		if (p->p_flag & P_TRACED) {
			error = EBUSY;
			goto out;
		}

		/* Can't trace yourself! */
		if (p->p_pid == td->td_proc->p_pid) {
			error = EINVAL;
			goto out;
		}

		/*
		 * Go ahead and set the trace flag.
		 * Save the old parent (it's reset in
		 *   _DETACH, and also in kern_exit.c:wait4()
		 * Reparent the process so that the tracing
		 *   proc gets to see all the action.
		 * Stop the target.
		 */
		p->p_flag |= P_TRACED;
		mtx_lock_spin(&sched_lock);
		faultin(p);
		mtx_unlock_spin(&sched_lock);
		p->p_xstat = 0;		/* XXX ? */
		if (p->p_pptr != td->td_proc) {
			p->p_oppid = p->p_pptr->p_pid;
			proc_reparent(p, td->td_proc);
		}
		psignal(p, SIGSTOP);
out:
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		return (error);
	}

	/*
	 * Authorization check: rely on normal debugging protection, except
	 * allow processes to disengage debugging on a process onto which
	 * they have previously attached, but no longer have permission to
	 * debug.
	 */
	PROC_LOCK(p);
	if (op != PROCFS_CTL_DETACH &&
	    ((error = p_candebug(td, p)))) {
		PROC_UNLOCK(p);
		return (error);
	}

	/*
	 * Target process must be stopped, owned by (td) and
	 * be set up for tracing (P_TRACED flag set).
	 * Allow DETACH to take place at any time for sanity.
	 * Allow WAIT any time, of course.
	 */
	switch (op) {
	case PROCFS_CTL_DETACH:
	case PROCFS_CTL_WAIT:
		break;

	default:
		if (!TRACE_WAIT_P(td->td_proc, p)) {
			PROC_UNLOCK(p);
			return (EBUSY);
		}
	}


#ifdef FIX_SSTEP
	/*
	 * do single-step fixup if needed
	 */
	FIX_SSTEP(FIRST_THREAD_IN_PROC(p));	/* XXXKSE */
#endif

	/*
	 * Don't deliver any signal by default.
	 * To continue with a signal, just send
	 * the signal name to the ctl file
	 */
	p->p_xstat = 0;

	switch (op) {
	/*
	 * Detach.  Cleans up the target process, reparent it if possible
	 * and set it running once more.
	 */
	case PROCFS_CTL_DETACH:
		/* if not being traced, then this is a painless no-op */
		if ((p->p_flag & P_TRACED) == 0) {
			PROC_UNLOCK(p);
			return (0);
		}

		/* not being traced any more */
		p->p_flag &= ~P_TRACED;

		/* remove pending SIGTRAP, else the process will die */
		SIGDELSET(p->p_siglist, SIGTRAP);
		PROC_UNLOCK(p);

		/* give process back to original parent */
		sx_xlock(&proctree_lock);
		if (p->p_oppid != p->p_pptr->p_pid) {
			struct proc *pp;

			pp = pfind(p->p_oppid);
			PROC_LOCK(p);
			if (pp) {
				PROC_UNLOCK(pp);
				proc_reparent(p, pp);
			}
		} else
			PROC_LOCK(p);
		p->p_oppid = 0;
		p->p_flag &= ~P_WAITED;	/* XXX ? */
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);

		wakeup((caddr_t) td->td_proc);	/* XXX for CTL_WAIT below ? */

		break;

	/*
	 * Step.  Let the target process execute a single instruction.
	 * What does it mean to single step a threaded program? 
	 */
	case PROCFS_CTL_STEP:
		PROC_UNLOCK(p);
		error = proc_sstep(FIRST_THREAD_IN_PROC(p)); /* XXXKSE */
		PRELE(p);
		if (error)
			return (error);
		break;

	/*
	 * Run.  Let the target process continue running until a breakpoint
	 * or some other trap.
	 */
	case PROCFS_CTL_RUN:
		PROC_UNLOCK(p);
		p->p_flag &= ~P_STOPPED_SIG;	/* this uses SIGSTOP */
		break;

	/*
	 * Wait for the target process to stop.
	 * If the target is not being traced then just wait
	 * to enter
	 */
	case PROCFS_CTL_WAIT:
		if (p->p_flag & P_TRACED) {
			while (error == 0 &&
					(P_SHOULDSTOP(p)) &&
					(p->p_flag & P_TRACED) &&
					(p->p_pptr == td->td_proc))
				error = msleep((caddr_t) p, &p->p_mtx,
						PWAIT|PCATCH, "procfsx", 0);
			if (error == 0 && !TRACE_WAIT_P(td->td_proc, p))
				error = EBUSY;
		} else {
			while (error == 0 && P_SHOULDSTOP(p))
				error = msleep((caddr_t) p, &p->p_mtx,
						PWAIT|PCATCH, "procfs", 0);
		}
		PROC_UNLOCK(p);
		return (error);
	default:
		panic("procfs_control");
	}

	mtx_lock_spin(&sched_lock);
	thread_unsuspend(p); /* If it can run, let it do so. */
	mtx_unlock_spin(&sched_lock);
	return (0);
}

static struct namemap *
findname(struct namemap *nm, char *buf, int buflen)
{

	for (; nm->nm_name; nm++)
		if (bcmp(buf, nm->nm_name, buflen+1) == 0)
			return (nm);

	return (0);
}

int
procfs_doprocctl(PFS_FILL_ARGS)
{
	int error;
	struct namemap *nm;

	if (uio == NULL || uio->uio_rw != UIO_WRITE)
		return (EOPNOTSUPP);

	/*
	 * Map signal names into signal generation
	 * or debug control.  Unknown commands and/or signals
	 * return EOPNOTSUPP.
	 *
	 * Sending a signal while the process is being debugged
	 * also has the side effect of letting the target continue
	 * to run.  There is no way to single-step a signal delivery.
	 */
	error = EOPNOTSUPP;

	sbuf_trim(sb);
	sbuf_finish(sb);
	nm = findname(ctlnames, sbuf_data(sb), sbuf_len(sb));
	if (nm) {
		printf("procfs: got a %s command\n", sbuf_data(sb));
		error = procfs_control(td, p, nm->nm_val);
	} else {
		nm = findname(signames, sbuf_data(sb), sbuf_len(sb));
		if (nm) {
			printf("procfs: got a sig%s\n", sbuf_data(sb));
			PROC_LOCK(p);

			/* This is very broken XXXKSE: */
			if (TRACE_WAIT_P(td->td_proc, p)) {
				p->p_xstat = nm->nm_val;
#ifdef FIX_SSTEP
				/* XXXKSE: */
				FIX_SSTEP(FIRST_THREAD_IN_PROC(p));
#endif
				mtx_lock_spin(&sched_lock);
				/* XXXKSE: */
				p->p_flag &= ~P_STOPPED_SIG;
				setrunnable(FIRST_THREAD_IN_PROC(p));
				mtx_unlock_spin(&sched_lock);
			} else
				psignal(p, nm->nm_val);
			PROC_UNLOCK(p);
			error = 0;
		}
	}

	return (error);
}
