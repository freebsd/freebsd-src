/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	$Id: jobs.c,v 1.14 1997/05/19 00:18:42 steve Exp $
 */

#ifndef lint
static char const sccsid[] = "@(#)jobs.c	8.5 (Berkeley) 5/4/95";
#endif /* not lint */

#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef BSD
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <sys/ioctl.h>

#include "shell.h"
#if JOBS
#if OLD_TTY_DRIVER
#include "sgtty.h"
#else
#include <termios.h>
#endif
#undef CEOF			/* syntax.h redefines this */
#endif
#include "redir.h"
#include "show.h"
#include "main.h"
#include "parser.h"
#include "nodes.h"
#include "jobs.h"
#include "options.h"
#include "trap.h"
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"


struct job *jobtab;		/* array of jobs */
int njobs;			/* size of array */
MKINIT short backgndpid = -1;	/* pid of last background process */
#if JOBS
int initialpgrp;		/* pgrp of shell on invocation */
short curjob;			/* current job */
#endif

#if JOBS
STATIC void restartjob __P((struct job *));
#endif
STATIC void freejob __P((struct job *));
STATIC struct job *getjob __P((char *));
STATIC int dowait __P((int, struct job *));
#if SYSV
STATIC int onsigchild __P((void));
#endif
STATIC int waitproc __P((int, int *));
STATIC void cmdtxt __P((union node *));
STATIC void cmdputs __P((char *));


/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 */

MKINIT int jobctl;

#if JOBS
void
setjobctl(on)
	int on;
{
#ifdef OLD_TTY_DRIVER
	int ldisc;
#endif

	if (on == jobctl || rootshell == 0)
		return;
	if (on) {
		do { /* while we are in the background */
#ifdef OLD_TTY_DRIVER
			if (ioctl(2, TIOCGPGRP, (char *)&initialpgrp) < 0) {
#else
			initialpgrp = tcgetpgrp(2);
			if (initialpgrp < 0) {
#endif
				out2str("sh: can't access tty; job control turned off\n");
				mflag = 0;
				return;
			}
			if (initialpgrp == -1)
				initialpgrp = getpgrp();
			else if (initialpgrp != getpgrp()) {
				killpg(initialpgrp, SIGTTIN);
				continue;
			}
		} while (0);
#ifdef OLD_TTY_DRIVER
		if (ioctl(2, TIOCGETD, (char *)&ldisc) < 0 || ldisc != NTTYDISC) {
			out2str("sh: need new tty driver to run job control; job control turned off\n");
			mflag = 0;
			return;
		}
#endif
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		setpgid(0, rootpid);
#ifdef OLD_TTY_DRIVER
		ioctl(2, TIOCSPGRP, (char *)&rootpid);
#else
		tcsetpgrp(2, rootpid);
#endif
	} else { /* turning job control off */
		setpgid(0, initialpgrp);
#ifdef OLD_TTY_DRIVER
		ioctl(2, TIOCSPGRP, (char *)&initialpgrp);
#else
		tcsetpgrp(2, initialpgrp);
#endif
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
	}
	jobctl = on;
}
#endif


#ifdef mkinit
INCLUDE <stdlib.h>

SHELLPROC {
	backgndpid = -1;
#if JOBS
	jobctl = 0;
#endif
}

#endif



#if JOBS
int
fgcmd(argc, argv)
	int argc __unused;
	char **argv;
{
	struct job *jp;
	int pgrp;
	int status;

	jp = getjob(argv[1]);
	if (jp->jobctl == 0)
		error("job not created under job control");
	pgrp = jp->ps[0].pid;
#ifdef OLD_TTY_DRIVER
	ioctl(2, TIOCSPGRP, (char *)&pgrp);
#else
	tcsetpgrp(2, pgrp);
#endif
	restartjob(jp);
	INTOFF;
	status = waitforjob(jp);
	INTON;
	return status;
}


int
bgcmd(argc, argv)
	int argc;
	char **argv;
{
	struct job *jp;

	do {
		jp = getjob(*++argv);
		if (jp->jobctl == 0)
			error("job not created under job control");
		restartjob(jp);
	} while (--argc > 1);
	return 0;
}


STATIC void
restartjob(jp)
	struct job *jp;
{
	struct procstat *ps;
	int i;

	if (jp->state == JOBDONE)
		return;
	INTOFF;
	killpg(jp->ps[0].pid, SIGCONT);
	for (ps = jp->ps, i = jp->nprocs ; --i >= 0 ; ps++) {
		if (WIFSTOPPED(ps->status)) {
			ps->status = -1;
			jp->state = 0;
		}
	}
	INTON;
}
#endif


int
jobscmd(argc, argv)
	int argc __unused;
	char **argv __unused;
{
	showjobs(0);
	return 0;
}


/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 *
 * If the shell is interrupted in the process of creating a job, the
 * result may be a job structure containing zero processes.  Such structures
 * will be freed here.
 */

void
showjobs(change)
	int change;
{
	int jobno;
	int procno;
	int i;
	struct job *jp;
	struct procstat *ps;
	int col;
	char s[64];

	TRACE(("showjobs(%d) called\n", change));
	while (dowait(0, (struct job *)NULL) > 0);
	for (jobno = 1, jp = jobtab ; jobno <= njobs ; jobno++, jp++) {
		if (! jp->used)
			continue;
		if (jp->nprocs == 0) {
			freejob(jp);
			continue;
		}
		if (change && ! jp->changed)
			continue;
		procno = jp->nprocs;
		for (ps = jp->ps ; ; ps++) {	/* for each process */
			if (ps == jp->ps)
				fmtstr(s, 64, "[%d] %d ", jobno, ps->pid);
			else
				fmtstr(s, 64, "    %d ", ps->pid);
			out1str(s);
			col = strlen(s);
			s[0] = '\0';
			if (ps->status == -1) {
				/* don't print anything */
			} else if (WIFEXITED(ps->status)) {
				fmtstr(s, 64, "Exit %d", WEXITSTATUS(ps->status));
			} else {
#if JOBS
				if (WIFSTOPPED(ps->status)) 
					i = WSTOPSIG(ps->status);
				else
#endif
					i = WTERMSIG(ps->status);
				if ((i & 0x7F) < NSIG && sys_siglist[i & 0x7F])
					scopy(sys_siglist[i & 0x7F], s);
				else
					fmtstr(s, 64, "Signal %d", i & 0x7F);
				if (WCOREDUMP(ps->status))
					strcat(s, " (core dumped)");
			}
			out1str(s);
			col += strlen(s);
			do {
				out1c(' ');
				col++;
			} while (col < 30);
			out1str(ps->cmd);
			out1c('\n');
			if (--procno <= 0)
				break;
		}
		jp->changed = 0;
		if (jp->state == JOBDONE) {
			freejob(jp);
		}
	}
}


/*
 * Mark a job structure as unused.
 */

STATIC void
freejob(jp)
	struct job *jp;
	{
	struct procstat *ps;
	int i;

	INTOFF;
	for (i = jp->nprocs, ps = jp->ps ; --i >= 0 ; ps++) {
		if (ps->cmd != nullstr)
			ckfree(ps->cmd);
	}
	if (jp->ps != &jp->ps0)
		ckfree(jp->ps);
	jp->used = 0;
#if JOBS
	if (curjob == jp - jobtab + 1)
		curjob = 0;
#endif
	INTON;
}



int
waitcmd(argc, argv)
	int argc;
	char **argv;
{
	struct job *job;
	int status, retval;
	struct job *jp;

	if (argc > 1) {
		job = getjob(argv[1]);
	} else {
		job = NULL;
	}
	for (;;) {	/* loop until process terminated or stopped */
		if (job != NULL) {
			if (job->state) {
				status = job->ps[job->nprocs - 1].status;
				if (WIFEXITED(status))
					retval = WEXITSTATUS(status);
#if JOBS
				else if (WIFSTOPPED(status))
					retval = WSTOPSIG(status) + 128;
#endif
				else
					retval = WTERMSIG(status) + 128;
				if (! iflag)
					freejob(job);
				return retval;
			}
		} else {
			for (jp = jobtab ; ; jp++) {
				if (jp >= jobtab + njobs) {	/* no running procs */
					return 0;
				}
				if (jp->used && jp->state == 0)
					break;
			}
		}
		dowait(1, (struct job *)NULL);
	}
}



int
jobidcmd(argc, argv)
	int argc __unused;
	char **argv;
{
	struct job *jp;
	int i;

	jp = getjob(argv[1]);
	for (i = 0 ; i < jp->nprocs ; ) {
		out1fmt("%d", jp->ps[i].pid);
		out1c(++i < jp->nprocs? ' ' : '\n');
	}
	return 0;
}



/*
 * Convert a job name to a job structure.
 */

STATIC struct job *
getjob(name)
	char *name;
	{
	int jobno;
	struct job *jp;
	int pid;
	int i;

	if (name == NULL) {
#if JOBS
currentjob:
		if ((jobno = curjob) == 0 || jobtab[jobno - 1].used == 0)
			error("No current job");
		return &jobtab[jobno - 1];
#else
		error("No current job");
#endif
	} else if (name[0] == '%') {
		if (is_digit(name[1])) {
			jobno = number(name + 1);
			if (jobno > 0 && jobno <= njobs
			 && jobtab[jobno - 1].used != 0)
				return &jobtab[jobno - 1];
#if JOBS
		} else if (name[1] == '%' && name[2] == '\0') {
			goto currentjob;
#endif
		} else {
			struct job *found = NULL;
			for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
				if (jp->used && jp->nprocs > 0
				 && prefix(name + 1, jp->ps[0].cmd)) {
					if (found)
						error("%s: ambiguous", name);
					found = jp;
				}
			}
			if (found)
				return found;
		}
	} else if (is_number(name)) {
		pid = number(name);
		for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
			if (jp->used && jp->nprocs > 0
			 && jp->ps[jp->nprocs - 1].pid == pid)
				return jp;
		}
	}
	error("No such job: %s", name);
	/*NOTREACHED*/
	return NULL;
}



/*
 * Return a new job structure,
 */

struct job *
makejob(node, nprocs)
	union node *node __unused;
	int nprocs;
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab ; ; jp++) {
		if (--i < 0) {
			INTOFF;
			if (njobs == 0) {
				jobtab = ckmalloc(4 * sizeof jobtab[0]);
			} else {
				jp = ckmalloc((njobs + 4) * sizeof jobtab[0]);
				memcpy(jp, jobtab, njobs * sizeof jp[0]);
				/* Relocate `ps' pointers */
				for (i = 0; i < njobs; i++)
					if (jp[i].ps == &jobtab[i].ps0)
						jp[i].ps = &jp[i].ps0;
				ckfree(jobtab);
				jobtab = jp;
			}
			jp = jobtab + njobs;
			for (i = 4 ; --i >= 0 ; jobtab[njobs++].used = 0);
			INTON;
			break;
		}
		if (jp->used == 0)
			break;
	}
	INTOFF;
	jp->state = 0;
	jp->used = 1;
	jp->changed = 0;
	jp->nprocs = 0;
#if JOBS
	jp->jobctl = jobctl;
#endif
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof (struct procstat));
	} else {
		jp->ps = &jp->ps0;
	}
	INTON;
	TRACE(("makejob(0x%lx, %d) returns %%%d\n", (long)node, nprocs,
	    jp - jobtab + 1));
	return jp;
}


/*
 * Fork of a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *	FORK_FG - Fork off a foreground process.
 *	FORK_BG - Fork off a background process.
 *	FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *		     process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 */

int
forkshell(jp, n, mode)
	union node *n;
	struct job *jp;
	int mode;
{
	int pid;
	int pgrp;

	TRACE(("forkshell(%%%d, 0x%lx, %d) called\n", jp - jobtab, (long)n,
	    mode));
	INTOFF;
	pid = fork();
	if (pid == -1) {
		TRACE(("Fork failed, errno=%d\n", errno));
		INTON;
		error("Cannot fork");
	}
	if (pid == 0) {
		struct job *p;
		int wasroot;
		int i;

		TRACE(("Child shell %d\n", getpid()));
		wasroot = rootshell;
		rootshell = 0;
		for (i = njobs, p = jobtab ; --i >= 0 ; p++)
			if (p->used)
				freejob(p);
		closescript();
		INTON;
		clear_traps();
#if JOBS
		jobctl = 0;		/* do job control only in root shell */
		if (wasroot && mode != FORK_NOJOB && mflag) {
			if (jp == NULL || jp->nprocs == 0)
				pgrp = getpid();
			else
				pgrp = jp->ps[0].pid;
			if (setpgid(0, pgrp) == 0 && mode == FORK_FG) {
				/*** this causes superfluous TIOCSPGRPS ***/
#ifdef OLD_TTY_DRIVER
				if (ioctl(2, TIOCSPGRP, (char *)&pgrp) < 0)
					error("TIOCSPGRP failed, errno=%d", errno);
#else
				if (tcsetpgrp(2, pgrp) < 0)
					error("tcsetpgrp failed, errno=%d", errno);
#endif
			}
			setsignal(SIGTSTP);
			setsignal(SIGTTOU);
		} else if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open("/dev/null", O_RDONLY) != 0)
					error("Can't open /dev/null");
			}
		}
#else
		if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open("/dev/null", O_RDONLY) != 0)
					error("Can't open /dev/null");
			}
		}
#endif
		if (wasroot && iflag) {
			setsignal(SIGINT);
			setsignal(SIGQUIT);
			setsignal(SIGTERM);
		}
		return pid;
	}
	if (rootshell && mode != FORK_NOJOB && mflag) {
		if (jp == NULL || jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].pid;
		setpgid(pid, pgrp);
	}
	if (mode == FORK_BG)
		backgndpid = pid;		/* set $! */
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->pid = pid;
		ps->status = -1;
		ps->cmd = nullstr;
		if (iflag && rootshell && n)
			ps->cmd = commandtext(n);
	}
	INTON;
	TRACE(("In parent shell:  child = %d\n", pid));
	return pid;
}



/*
 * Wait for job to finish.
 *
 * Under job control we have the problem that while a child process is
 * running interrupts generated by the user are sent to the child but not
 * to the shell.  This means that an infinite loop started by an inter-
 * active user may be hard to kill.  With job control turned off, an
 * interactive user may place an interactive program inside a loop.  If
 * the interactive program catches interrupts, the user doesn't want
 * these interrupts to also abort the loop.  The approach we take here
 * is to have the shell ignore interrupt signals while waiting for a
 * forground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 */

int
waitforjob(jp)
	struct job *jp;
	{
#if JOBS
	int mypgrp = getpgrp();
#endif
	int status;
	int st;

	INTOFF;
	TRACE(("waitforjob(%%%d) called\n", jp - jobtab + 1));
	while (jp->state == 0) {
		dowait(1, jp);
	}
#if JOBS
	if (jp->jobctl) {
#ifdef OLD_TTY_DRIVER
		if (ioctl(2, TIOCSPGRP, (char *)&mypgrp) < 0)
			error("TIOCSPGRP failed, errno=%d\n", errno);
#else
		if (tcsetpgrp(2, mypgrp) < 0)
			error("tcsetpgrp failed, errno=%d\n", errno);
#endif
	}
	if (jp->state == JOBSTOPPED)
		curjob = jp - jobtab + 1;
#endif
	status = jp->ps[jp->nprocs - 1].status;
	/* convert to 8 bits */
	if (WIFEXITED(status))
		st = WEXITSTATUS(status);
#if JOBS
	else if (WIFSTOPPED(status))
		st = WSTOPSIG(status) + 128;
#endif
	else
		st = WTERMSIG(status) + 128;
	if (! JOBS || jp->state == JOBDONE)
		freejob(jp);
	CLEAR_PENDING_INT;
	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
		kill(getpid(), SIGINT);
	INTON;
	return st;
}



/*
 * Wait for a process to terminate.
 */

STATIC int
dowait(block, job)
	int block;
	struct job *job;
{
	int pid;
	int status;
	struct procstat *sp;
	struct job *jp;
	struct job *thisjob;
	int done;
	int stopped;
	int core;
	int sig;

	TRACE(("dowait(%d) called\n", block));
	do {
		pid = waitproc(block, &status);
		TRACE(("wait returns %d, status=%d\n", pid, status));
	} while (pid == -1 && errno == EINTR);
	if (pid <= 0)
		return pid;
	INTOFF;
	thisjob = NULL;
	for (jp = jobtab ; jp < jobtab + njobs ; jp++) {
		if (jp->used) {
			done = 1;
			stopped = 1;
			for (sp = jp->ps ; sp < jp->ps + jp->nprocs ; sp++) {
				if (sp->pid == -1)
					continue;
				if (sp->pid == pid) {
					TRACE(("Changing status of proc %d from 0x%x to 0x%x\n",
						   pid, sp->status, status));
					sp->status = status;
					thisjob = jp;
				}
				if (sp->status == -1)
					stopped = 0;
				else if (WIFSTOPPED(sp->status))
					done = 0;
			}
			if (stopped) {		/* stopped or done */
				int state = done? JOBDONE : JOBSTOPPED;
				if (jp->state != state) {
					TRACE(("Job %d: changing state from %d to %d\n", jp - jobtab + 1, jp->state, state));
					jp->state = state;
#if JOBS
					if (done && curjob == jp - jobtab + 1)
						curjob = 0;		/* no current job */
#endif
				}
			}
		}
	}
	INTON;
	if (! rootshell || ! iflag || (job && thisjob == job)) {
		core = WCOREDUMP(status);
#if JOBS
		if (WIFSTOPPED(status))
			sig = WSTOPSIG(status);
		else
#endif
			if (WIFEXITED(status))
				sig = 0;
			else
				sig = WTERMSIG(status);

		if (sig != 0 && sig != SIGINT && sig != SIGPIPE) {
			if (thisjob != job)
				outfmt(out2, "%d: ", pid);
#if JOBS
			if (sig == SIGTSTP && rootshell && iflag)
				outfmt(out2, "%%%d ", job - jobtab + 1);
#endif
			if (sig < NSIG && sys_siglist[sig])
				out2str(sys_siglist[sig]);
			else
				outfmt(out2, "Signal %d", sig);
			if (core)
				out2str(" - core dumped");
			out2c('\n');
			flushout(&errout);
		} else {
			TRACE(("Not printing status: status=%d, sig=%d\n", 
				   status, sig));
		}
	} else {
		TRACE(("Not printing status, rootshell=%d, job=0x%x\n", rootshell, job));
		if (thisjob)
			thisjob->changed = 1;
	}
	return pid;
}



/*
 * Do a wait system call.  If job control is compiled in, we accept
 * stopped processes.  If block is zero, we return a value of zero
 * rather than blocking.
 *
 * System V doesn't have a non-blocking wait system call.  It does
 * have a SIGCLD signal that is sent to a process when one of it's
 * children dies.  The obvious way to use SIGCLD would be to install
 * a handler for SIGCLD which simply bumped a counter when a SIGCLD
 * was received, and have waitproc bump another counter when it got
 * the status of a process.  Waitproc would then know that a wait
 * system call would not block if the two counters were different.
 * This approach doesn't work because if a process has children that
 * have not been waited for, System V will send it a SIGCLD when it
 * installs a signal handler for SIGCLD.  What this means is that when
 * a child exits, the shell will be sent SIGCLD signals continuously
 * until is runs out of stack space, unless it does a wait call before
 * restoring the signal handler.  The code below takes advantage of
 * this (mis)feature by installing a signal handler for SIGCLD and
 * then checking to see whether it was called.  If there are any
 * children to be waited for, it will be.
 *
 * If neither SYSV nor BSD is defined, we don't implement nonblocking
 * waits at all.  In this case, the user will not be informed when
 * a background process until the next time she runs a real program
 * (as opposed to running a builtin command or just typing return),
 * and the jobs command may give out of date information.
 */

#ifdef SYSV
STATIC int gotsigchild;

STATIC int onsigchild() {
	gotsigchild = 1;
}
#endif


STATIC int
waitproc(block, status)
	int block;
	int *status;
{
#ifdef BSD
	int flags;

#if JOBS
	flags = WUNTRACED;
#else
	flags = 0;
#endif
	if (block == 0)
		flags |= WNOHANG;
	return wait3(status, flags, (struct rusage *)NULL);
#else
#ifdef SYSV
	int (*save)();

	if (block == 0) {
		gotsigchild = 0;
		save = signal(SIGCLD, onsigchild);
		signal(SIGCLD, save);
		if (gotsigchild == 0)
			return 0;
	}
	return wait(status);
#else
	if (block == 0)
		return 0;
	return wait(status);
#endif
#endif
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
int job_warning = 0;
int
stoppedjobs()
{
	int jobno;
	struct job *jp;

	if (job_warning)
		return (0);
	for (jobno = 1, jp = jobtab; jobno <= njobs; jobno++, jp++) {
		if (jp->used == 0)
			continue;
		if (jp->state == JOBSTOPPED) {
			out2str("You have stopped jobs.\n");
			job_warning = 2;
			return (1);
		}
	}

	return (0);
}

/*
 * Return a string identifying a command (to be printed by the
 * jobs command.
 */

STATIC char *cmdnextc;
STATIC int cmdnleft;
STATIC void cmdtxt(), cmdputs();
#define MAXCMDTEXT	200

char *
commandtext(n)
	union node *n;
	{
	char *name;

	cmdnextc = name = ckmalloc(MAXCMDTEXT);
	cmdnleft = MAXCMDTEXT - 4;
	cmdtxt(n);
	*cmdnextc = '\0';
	return name;
}


STATIC void
cmdtxt(n)
	union node *n;
	{
	union node *np;
	struct nodelist *lp;
	char *p;
	int i;
	char s[2];

	if (n == NULL)
		return;
	switch (n->type) {
	case NSEMI:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NAND:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" && ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NOR:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" || ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
			cmdtxt(lp->n);
			if (lp->next)
				cmdputs(" | ");
		}
		break;
	case NSUBSHELL:
		cmdputs("(");
		cmdtxt(n->nredir.n);
		cmdputs(")");
		break;
	case NREDIR:
	case NBACKGND:
		cmdtxt(n->nredir.n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		cmdtxt(n->nif.ifpart);
		cmdputs("...");
		break;
	case NWHILE:
		cmdputs("while ");
		goto until;
	case NUNTIL:
		cmdputs("until ");
until:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; do ");
		cmdtxt(n->nbinary.ch2);
		cmdputs("; done");
		break;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ...");
		break;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ...");
		break;
	case NDEFUN:
		cmdputs(n->narg.text);
		cmdputs("() ...");
		break;
	case NCMD:
		for (np = n->ncmd.args ; np ; np = np->narg.next) {
			cmdtxt(np);
			if (np->narg.next)
				cmdputs(" ");
		}
		for (np = n->ncmd.redirect ; np ; np = np->nfile.next) {
			cmdputs(" ");
			cmdtxt(np);
		}
		break;
	case NARG:
		cmdputs(n->narg.text);
		break;
	case NTO:
		p = ">";  i = 1;  goto redir;
	case NAPPEND:
		p = ">>";  i = 1;  goto redir;
	case NTOFD:
		p = ">&";  i = 1;  goto redir;
	case NFROM:
		p = "<";  i = 0;  goto redir;
	case NFROMFD:
		p = "<&";  i = 0;  goto redir;
redir:
		if (n->nfile.fd != i) {
			s[0] = n->nfile.fd + '0';
			s[1] = '\0';
			cmdputs(s);
		}
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			s[0] = n->ndup.dupfd + '0';
			s[1] = '\0';
			cmdputs(s);
		} else {
			cmdtxt(n->nfile.fname);
		}
		break;
	case NHERE:
	case NXHERE:
		cmdputs("<<...");
		break;
	default:
		cmdputs("???");
		break;
	}
}



STATIC void
cmdputs(s)
	char *s;
	{
	char *p, *q;
	char c;
	int subtype = 0;

	if (cmdnleft <= 0)
		return;
	p = s;
	q = cmdnextc;
	while ((c = *p++) != '\0') {
		if (c == CTLESC)
			*q++ = *p++;
		else if (c == CTLVAR) {
			*q++ = '$';
			if (--cmdnleft > 0)
				*q++ = '{';
			subtype = *p++;
		} else if (c == '=' && subtype != 0) {
			*q++ = "}-+?="[(subtype & VSTYPE) - VSNORMAL];
			subtype = 0;
		} else if (c == CTLENDVAR) {
			*q++ = '}';
		} else if (c == CTLBACKQ || c == CTLBACKQ+CTLQUOTE)
			cmdnleft++;		/* ignore it */
		else
			*q++ = c;
		if (--cmdnleft <= 0) {
			*q++ = '.';
			*q++ = '.';
			*q++ = '.';
			break;
		}
	}
	cmdnextc = q;
}
