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
	((p)->p_stat == SSTOP && \
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

static int	procfs_control __P((struct proc *curp, struct proc *p, int op));

static int
procfs_control(struct proc *curp, struct proc *p, int op)
{
	int error = 0;

	/*
	 * Authorization check: rely on normal debugging protection, except
	 * allow processes to disengage debugging on a process onto which
	 * they have previously attached, but no longer have permission to
	 * debug.
	 */
	if (op != PROCFS_CTL_DETACH &&
	    ((error = p_candebug(curp, p))))
		return (error);

	/*
	 * Attach - attaches the target process for debugging
	 * by the calling process.
	 */
	if (op == PROCFS_CTL_ATTACH) {
		sx_xlock(&proctree_lock);
		PROC_LOCK(p);
		if (p->p_flag & P_TRACED) {
			error = EBUSY;
			goto out;
		}

		/* Can't trace yourself! */
		if (p->p_pid == curp->p_pid) {
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
		faultin(p);
		p->p_xstat = 0;		/* XXX ? */
		if (p->p_pptr != curp) {
			p->p_oppid = p->p_pptr->p_pid;
			proc_reparent(p, curp);
		}
		psignal(p, SIGSTOP);
out:
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		return (error);
	}

	/*
	 * Target process must be stopped, owned by (curp) and
	 * be set up for tracing (P_TRACED flag set).
	 * Allow DETACH to take place at any time for sanity.
	 * Allow WAIT any time, of course.
	 */
	switch (op) {
	case PROCFS_CTL_DETACH:
	case PROCFS_CTL_WAIT:
		break;

	default:
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		if (!TRACE_WAIT_P(curp, p)) {
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
			return (EBUSY);
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	}


#ifdef FIX_SSTEP
	/*
	 * do single-step fixup if needed
	 */
	FIX_SSTEP(&p->p_thread);		/* XXXKSE */
#endif

	/*
	 * Don't deliver any signal by default.
	 * To continue with a signal, just send
	 * the signal name to the ctl file
	 */
	PROC_LOCK(p);
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
			if (pp)
				proc_reparent(p, pp);
		} else
			PROC_LOCK(p);
		p->p_oppid = 0;
		p->p_flag &= ~P_WAITED;	/* XXX ? */
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);

		wakeup((caddr_t) curp);	/* XXX for CTL_WAIT below ? */

		break;

	/*
	 * Step.  Let the target process execute a single instruction.
	 */
	case PROCFS_CTL_STEP:
		PROC_UNLOCK(p);
		error = proc_sstep(&p->p_thread); /* XXXKSE */
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
		break;

	/*
	 * Wait for the target process to stop.
	 * If the target is not being traced then just wait
	 * to enter
	 */
	case PROCFS_CTL_WAIT:
		if (p->p_flag & P_TRACED) {
			mtx_lock_spin(&sched_lock);
			while (error == 0 &&
					(p->p_stat != SSTOP) &&
					(p->p_flag & P_TRACED) &&
					(p->p_pptr == curp)) {
				mtx_unlock_spin(&sched_lock);
				error = msleep((caddr_t) p, &p->p_mtx,
						PWAIT|PCATCH, "procfsx", 0);
				mtx_lock_spin(&sched_lock);
			}
			if (error == 0 && !TRACE_WAIT_P(curp, p))
				error = EBUSY;
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
		} else {
			PROC_UNLOCK(p);
			mtx_lock_spin(&sched_lock);
			while (error == 0 && p->p_stat != SSTOP) {
				mtx_unlock_spin(&sched_lock);
				error = tsleep((caddr_t) p,
						PWAIT|PCATCH, "procfs", 0);
				mtx_lock_spin(&sched_lock);
			}
			mtx_unlock_spin(&sched_lock);
		}
		return (error);

	default:
		panic("procfs_control");
	}

	mtx_lock_spin(&sched_lock);
	if (p->p_stat == SSTOP)
		setrunnable(&p->p_thread); /* XXXKSE */
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
		error = procfs_control(td->td_proc, p, nm->nm_val);
	} else {
		nm = findname(signames, sbuf_data(sb), sbuf_len(sb));
		if (nm) {
			printf("procfs: got a sig%s\n", sbuf_data(sb));
			PROC_LOCK(p);
			mtx_lock_spin(&sched_lock);
			if (TRACE_WAIT_P(td->td_proc, p)) {
				p->p_xstat = nm->nm_val;
#ifdef FIX_SSTEP
				FIX_SSTEP(&p->p_thread);   /* XXXKSE */
#endif
				setrunnable(&p->p_thread); /* XXXKSE */
				mtx_unlock_spin(&sched_lock);
			} else {
				mtx_unlock_spin(&sched_lock);
				psignal(p, nm->nm_val);
			}
			PROC_UNLOCK(p);
			error = 0;
		}
	}

	return (error);
}
