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
 */

#ifndef lint
static char sccsid[] = "@(#)jobs.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "shell.h"
#if JOBS
#include "sgtty.h"
#undef CEOF			/* syntax.h redefines this */
#endif
#include "main.h"
#include "parser.h"
#include "nodes.h"
#include "jobs.h"
#include "options.h"
#include "trap.h"
#include "signames.h"
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#ifdef BSD
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif



struct job *jobtab;		/* array of jobs */
int njobs;			/* size of array */
MKINIT short backgndpid = -1;	/* pid of last background process */
#if JOBS
int initialpgrp;		/* pgrp of shell on invocation */
short curjob;			/* current job */
#endif

#ifdef __STDC__
STATIC void restartjob(struct job *);
STATIC struct job *getjob(char *);
STATIC void freejob(struct job *);
STATIC int procrunning(int);
STATIC int dowait(int, struct job *);
STATIC int waitproc(int, int *);
#else
STATIC void restartjob();
STATIC struct job *getjob();
STATIC void freejob();
STATIC int procrunning();
STATIC int dowait();
STATIC int waitproc();
#endif



/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 */

MKINIT int jobctl;

void
setjobctl(on) {
#ifdef OLD_TTY_DRIVER
	int ldisc;
#endif

	if (on == jobctl || rootshell == 0)
		return;
	if (on) {
		do { /* while we are in the background */
			if (ioctl(2, TIOCGPGRP, (char *)&initialpgrp) < 0) {
				out2str("sh: can't access tty; job control turned off\n");
				mflag = 0;
				return;
			}
			if (initialpgrp == -1)
				initialpgrp = getpgrp(0);
			else if (initialpgrp != getpgrp(0)) {
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
		setpgrp(0, rootpid);
		ioctl(2, TIOCSPGRP, (char *)&rootpid);
	} else { /* turning job control off */
		setpgrp(0, initialpgrp);
		ioctl(2, TIOCSPGRP, (char *)&initialpgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
	}
	jobctl = on;
}


#ifdef mkinit

SHELLPROC {
	backgndpid = -1;
#if JOBS
	jobctl = 0;
#endif
}

#endif



#if JOBS
fgcmd(argc, argv)  char **argv; {
	struct job *jp;
	int pgrp;
	int status;

	jp = getjob(argv[1]);
	if (jp->jobctl == 0)
		error("job not created under job control");
	pgrp = jp->ps[0].pid;
	ioctl(2, TIOCSPGRP, (char *)&pgrp);
	restartjob(jp);
	INTOFF;
	status = waitforjob(jp);
	INTON;
	return status;
}


bgcmd(argc, argv)  char **argv; {
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
		if ((ps->status & 0377) == 0177) {
			ps->status = -1;
			jp->state = 0;
		}
	}
	INTON;
}
#endif


int
jobscmd(argc, argv)  char **argv; {
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
showjobs(change) {
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
			} else if ((ps->status & 0xFF) == 0) {
				fmtstr(s, 64, "Exit %d", ps->status >> 8);
			} else {
				i = ps->status;
#if JOBS
				if ((i & 0xFF) == 0177)
					i >>= 8;
#endif
				if ((i & 0x7F) <= MAXSIG && sigmesg[i & 0x7F])
					scopy(sigmesg[i & 0x7F], s);
				else
					fmtstr(s, 64, "Signal %d", i & 0x7F);
				if (i & 0x80)
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
waitcmd(argc, argv)  char **argv; {
	struct job *job;
	int status;
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
				if ((status & 0xFF) == 0)
					status = status >> 8 & 0xFF;
#if JOBS
				else if ((status & 0xFF) == 0177)
					status = (status >> 8 & 0x7F) + 128;
#endif
				else
					status = (status & 0x7F) + 128;
				if (! iflag)
					freejob(job);
				return status;
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



jobidcmd(argc, argv)  char **argv; {
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
	register struct job *jp;
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
			register struct job *found = NULL;
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
}



/*
 * Return a new job structure,
 */

struct job *
makejob(node, nprocs)
	union node *node;
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
				bcopy(jobtab, jp, njobs * sizeof jp[0]);
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
	TRACE(("makejob(0x%x, %d) returns %%%d\n", (int)node, nprocs, jp - jobtab + 1));
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
	{
	int pid;
	int pgrp;

	TRACE(("forkshell(%%%d, 0x%x, %d) called\n", jp - jobtab, (int)n, mode));
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
			setpgrp(0, pgrp);
			if (mode == FORK_FG) {
				/*** this causes superfluous TIOCSPGRPS ***/
				if (ioctl(2, TIOCSPGRP, (char *)&pgrp) < 0)
					error("TIOCSPGRP failed, errno=%d\n", errno);
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
		setpgrp(pid, pgrp);
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
	register struct job *jp;
	{
#if JOBS
	int mypgrp = getpgrp(0);
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
		if (ioctl(2, TIOCSPGRP, (char *)&mypgrp) < 0)
			error("TIOCSPGRP failed, errno=%d\n", errno);
	}
	if (jp->state == JOBSTOPPED)
		curjob = jp - jobtab + 1;
#endif
	status = jp->ps[jp->nprocs - 1].status;
	/* convert to 8 bits */
	if ((status & 0xFF) == 0)
		st = status >> 8 & 0xFF;
#if JOBS
	else if ((status & 0xFF) == 0177)
		st = (status >> 8 & 0x7F) + 128;
#endif
	else
		st = (status & 0x7F) + 128;
	if (! JOBS || jp->state == JOBDONE)
		freejob(jp);
	CLEAR_PENDING_INT;
	if ((status & 0x7F) == SIGINT)
		kill(getpid(), SIGINT);
	INTON;
	return st;
}



/*
 * Wait for a process to terminate.
 */

STATIC int
dowait(block, job)
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
					TRACE(("Changin status of proc %d from 0x%x to 0x%x\n", pid, sp->status, status));
					sp->status = status;
					thisjob = jp;
				}
				if (sp->status == -1)
					stopped = 0;
				else if ((sp->status & 0377) == 0177)
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
#if JOBS
		if ((status & 0xFF) == 0177)
			status >>= 8;
#endif
		core = status & 0x80;
		status &= 0x7F;
		if (status != 0 && status != SIGINT && status != SIGPIPE) {
			if (thisjob != job)
				outfmt(out2, "%d: ", pid);
#if JOBS
			if (status == SIGTSTP && rootshell && iflag)
				outfmt(out2, "%%%d ", job - jobtab + 1);
#endif
			if (status <= MAXSIG && sigmesg[status])
				out2str(sigmesg[status]);
			else
				outfmt(out2, "Signal %d", status);
			if (core)
				out2str(" - core dumped");
			out2c('\n');
			flushout(&errout);
		} else {
			TRACE(("Not printing status: status=%d\n", status));
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
	register int jobno;
	register struct job *jp;

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
	register char *p, *q;
	register char c;
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
		} else if (c == CTLBACKQ | c == CTLBACKQ+CTLQUOTE)
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
