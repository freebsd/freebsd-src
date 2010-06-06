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
#if 0
static char sccsid[] = "@(#)jobs.c	8.5 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <paths.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <paths.h>
#include <sys/ioctl.h>

#include "shell.h"
#if JOBS
#include <termios.h>
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


STATIC struct job *jobtab;	/* array of jobs */
STATIC int njobs;		/* size of array */
MKINIT pid_t backgndpid = -1;	/* pid of last background process */
#if JOBS
STATIC struct job *jobmru;	/* most recently used job list */
STATIC pid_t initialpgrp;	/* pgrp of shell on invocation */
#endif
int in_waitcmd = 0;		/* are we in waitcmd()? */
int in_dowait = 0;		/* are we in dowait()? */
volatile sig_atomic_t breakwaitcmd = 0;	/* should wait be terminated? */
static int ttyfd = -1;

#if JOBS
STATIC void restartjob(struct job *);
#endif
STATIC void freejob(struct job *);
STATIC struct job *getjob(char *);
STATIC pid_t dowait(int, struct job *);
STATIC pid_t waitproc(int, int *);
STATIC void checkzombies(void);
STATIC void cmdtxt(union node *);
STATIC void cmdputs(const char *);
#if JOBS
STATIC void setcurjob(struct job *);
STATIC void deljob(struct job *);
STATIC struct job *getcurjob(struct job *);
#endif
STATIC void showjob(struct job *, pid_t, int);


/*
 * Turn job control on and off.
 */

MKINIT int jobctl;

#if JOBS
void
setjobctl(int on)
{
	int i;

	if (on == jobctl || rootshell == 0)
		return;
	if (on) {
		if (ttyfd != -1)
			close(ttyfd);
		if ((ttyfd = open(_PATH_TTY, O_RDWR)) < 0) {
			i = 0;
			while (i <= 2 && !isatty(i))
				i++;
			if (i > 2 || (ttyfd = fcntl(i, F_DUPFD, 10)) < 0)
				goto out;
		}
		if (ttyfd < 10) {
			/*
			 * Keep our TTY file descriptor out of the way of
			 * the user's redirections.
			 */
			if ((i = fcntl(ttyfd, F_DUPFD, 10)) < 0) {
				close(ttyfd);
				ttyfd = -1;
				goto out;
			}
			close(ttyfd);
			ttyfd = i;
		}
		if (fcntl(ttyfd, F_SETFD, FD_CLOEXEC) < 0) {
			close(ttyfd);
			ttyfd = -1;
			goto out;
		}
		do { /* while we are in the background */
			initialpgrp = tcgetpgrp(ttyfd);
			if (initialpgrp < 0) {
out:				out2fmt_flush("sh: can't access tty; job control turned off\n");
				mflag = 0;
				return;
			}
			if (initialpgrp == -1)
				initialpgrp = getpgrp();
			else if (initialpgrp != getpgrp()) {
				killpg(0, SIGTTIN);
				continue;
			}
		} while (0);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		setpgid(0, rootpid);
		tcsetpgrp(ttyfd, rootpid);
	} else { /* turning job control off */
		setpgid(0, initialpgrp);
		tcsetpgrp(ttyfd, initialpgrp);
		close(ttyfd);
		ttyfd = -1;
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
	}
	jobctl = on;
}
#endif


#ifdef mkinit
INCLUDE <sys/types.h>
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
fgcmd(int argc __unused, char **argv)
{
	struct job *jp;
	pid_t pgrp;
	int status;

	jp = getjob(argv[1]);
	if (jp->jobctl == 0)
		error("job not created under job control");
	out1str(jp->ps[0].cmd);
	out1c('\n');
	flushout(&output);
	pgrp = jp->ps[0].pid;
	tcsetpgrp(ttyfd, pgrp);
	restartjob(jp);
	jp->foreground = 1;
	INTOFF;
	status = waitforjob(jp, (int *)NULL);
	INTON;
	return status;
}


int
bgcmd(int argc, char **argv)
{
	char s[64];
	struct job *jp;

	do {
		jp = getjob(*++argv);
		if (jp->jobctl == 0)
			error("job not created under job control");
		if (jp->state == JOBDONE)
			continue;
		restartjob(jp);
		jp->foreground = 0;
		fmtstr(s, 64, "[%td] ", jp - jobtab + 1);
		out1str(s);
		out1str(jp->ps[0].cmd);
		out1c('\n');
	} while (--argc > 1);
	return 0;
}


STATIC void
restartjob(struct job *jp)
{
	struct procstat *ps;
	int i;

	if (jp->state == JOBDONE)
		return;
	setcurjob(jp);
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
jobscmd(int argc, char *argv[])
{
	char *id;
	int ch, mode;

	optind = optreset = 1;
	opterr = 0;
	mode = SHOWJOBS_DEFAULT;
	while ((ch = getopt(argc, argv, "lps")) != -1) {
		switch (ch) {
		case 'l':
			mode = SHOWJOBS_VERBOSE;
			break;
		case 'p':
			mode = SHOWJOBS_PGIDS;
			break;
		case 's':
			mode = SHOWJOBS_PIDS;
			break;
		case '?':
		default:
			error("unknown option: -%c", optopt);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		showjobs(0, mode);
	else
		while ((id = *argv++) != NULL)
			showjob(getjob(id), 0, mode);

	return (0);
}

STATIC void
showjob(struct job *jp, pid_t pid, int mode)
{
	char s[64];
	struct procstat *ps;
	struct job *j;
	int col, curr, i, jobno, prev, procno;
	pid_t ppid;
	char c;

	procno = (mode == SHOWJOBS_PGIDS) ? 1 : jp->nprocs;
	jobno = jp - jobtab + 1;
	curr = prev = 0;
#if JOBS
	if ((j = getcurjob(NULL)) != NULL) {
		curr = j - jobtab + 1;
		if ((j = getcurjob(j)) != NULL)
			prev = j - jobtab + 1;
	}
#endif
	for (ps = jp->ps ; ; ps++) {	/* for each process */
		if (mode == SHOWJOBS_PIDS || mode == SHOWJOBS_PGIDS) {
			ppid = (mode == SHOWJOBS_PIDS) ? ps->pid :
			    getpgid(ps->pid);
			out1fmt("%d\n", (int)ppid);
			goto skip;
		}
		if (mode != SHOWJOBS_VERBOSE && ps != jp->ps && pid == 0)
			goto skip;
		if (pid != 0 && pid != ps->pid)
			goto skip;
		if (jobno == curr && ps == jp->ps)
			c = '+';
		else if (jobno == prev && ps == jp->ps)
			c = '-';
		else
			c = ' ';
		if (ps == jp->ps)
			fmtstr(s, 64, "[%d] %c ", jobno, c);
		else
			fmtstr(s, 64, "    %c ", c);
		out1str(s);
		col = strlen(s);
		if (mode == SHOWJOBS_VERBOSE) {
			fmtstr(s, 64, "%d ", (int)ps->pid);
			out1str(s);
			col += strlen(s);
		}
		s[0] = '\0';
		if (ps != jp->ps) {
			*s = '\0';
		} else if (ps->status == -1) {
			strcpy(s, "Running");
		} else if (WIFEXITED(ps->status)) {
			if (WEXITSTATUS(ps->status) == 0)
				strcpy(s, "Done");
			else
				fmtstr(s, 64, "Done (%d)",
				    WEXITSTATUS(ps->status));
		} else {
#if JOBS
			if (WIFSTOPPED(ps->status))
				i = WSTOPSIG(ps->status);
			else
#endif
				i = WTERMSIG(ps->status);
			if ((i & 0x7F) < sys_nsig && sys_siglist[i & 0x7F])
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
skip:		if (--procno <= 0)
			break;
	}
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
showjobs(int change, int mode)
{
	int jobno;
	struct job *jp;

	TRACE(("showjobs(%d) called\n", change));
	checkzombies();
	for (jobno = 1, jp = jobtab ; jobno <= njobs ; jobno++, jp++) {
		if (! jp->used)
			continue;
		if (jp->nprocs == 0) {
			freejob(jp);
			continue;
		}
		if (change && ! jp->changed)
			continue;
		showjob(jp, 0, mode);
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
freejob(struct job *jp)
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
	deljob(jp);
#endif
	INTON;
}



int
waitcmd(int argc, char **argv)
{
	struct job *job;
	int status, retval;
	struct job *jp;

	if (argc > 1) {
		job = getjob(argv[1]);
	} else {
		job = NULL;
	}

	/*
	 * Loop until a process is terminated or stopped, or a SIGINT is
	 * received.
	 */

	in_waitcmd++;
	do {
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
				in_waitcmd--;
				return retval;
			}
		} else {
			for (jp = jobtab ; ; jp++) {
				if (jp >= jobtab + njobs) {	/* no running procs */
					in_waitcmd--;
					return 0;
				}
				if (jp->used && jp->state == 0)
					break;
			}
		}
	} while (dowait(1, (struct job *)NULL) != -1);
	in_waitcmd--;

	return 0;
}



int
jobidcmd(int argc __unused, char **argv)
{
	struct job *jp;
	int i;

	jp = getjob(argv[1]);
	for (i = 0 ; i < jp->nprocs ; ) {
		out1fmt("%d", (int)jp->ps[i].pid);
		out1c(++i < jp->nprocs? ' ' : '\n');
	}
	return 0;
}



/*
 * Convert a job name to a job structure.
 */

STATIC struct job *
getjob(char *name)
{
	int jobno;
	struct job *found, *jp;
	pid_t pid;
	int i;

	if (name == NULL) {
#if JOBS
currentjob:	if ((jp = getcurjob(NULL)) == NULL)
			error("No current job");
		return (jp);
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
		} else if (name[1] == '+' && name[2] == '\0') {
			goto currentjob;
		} else if (name[1] == '-' && name[2] == '\0') {
			if ((jp = getcurjob(NULL)) == NULL ||
			    (jp = getcurjob(jp)) == NULL)
				error("No previous job");
			return (jp);
#endif
		} else if (name[1] == '?') {
			found = NULL;
			for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
				if (jp->used && jp->nprocs > 0
				 && strstr(jp->ps[0].cmd, name + 2) != NULL) {
					if (found)
						error("%s: ambiguous", name);
					found = jp;
				}
			}
			if (found != NULL)
				return (found);
		} else {
			found = NULL;
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
		pid = (pid_t)number(name);
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
makejob(union node *node __unused, int nprocs)
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab ; ; jp++) {
		if (--i < 0) {
			INTOFF;
			if (njobs == 0) {
				jobtab = ckmalloc(4 * sizeof jobtab[0]);
#if JOBS
				jobmru = NULL;
#endif
			} else {
				jp = ckmalloc((njobs + 4) * sizeof jobtab[0]);
				memcpy(jp, jobtab, njobs * sizeof jp[0]);
#if JOBS
				/* Relocate `next' pointers and list head */
				if (jobmru != NULL)
					jobmru = &jp[jobmru - jobtab];
				for (i = 0; i < njobs; i++)
					if (jp[i].next != NULL)
						jp[i].next = &jp[jp[i].next -
						    jobtab];
#endif
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
	jp->foreground = 0;
#if JOBS
	jp->jobctl = jobctl;
	jp->next = NULL;
#endif
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof (struct procstat));
	} else {
		jp->ps = &jp->ps0;
	}
	INTON;
	TRACE(("makejob(%p, %d) returns %%%d\n", (void *)node, nprocs,
	    jp - jobtab + 1));
	return jp;
}

#if JOBS
STATIC void
setcurjob(struct job *cj)
{
	struct job *jp, *prev;

	for (prev = NULL, jp = jobmru; jp != NULL; prev = jp, jp = jp->next) {
		if (jp == cj) {
			if (prev != NULL)
				prev->next = jp->next;
			else
				jobmru = jp->next;
			jp->next = jobmru;
			jobmru = cj;
			return;
		}
	}
	cj->next = jobmru;
	jobmru = cj;
}

STATIC void
deljob(struct job *j)
{
	struct job *jp, *prev;

	for (prev = NULL, jp = jobmru; jp != NULL; prev = jp, jp = jp->next) {
		if (jp == j) {
			if (prev != NULL)
				prev->next = jp->next;
			else
				jobmru = jp->next;
			return;
		}
	}
}

/*
 * Return the most recently used job that isn't `nj', and preferably one
 * that is stopped.
 */
STATIC struct job *
getcurjob(struct job *nj)
{
	struct job *jp;

	/* Try to find a stopped one.. */
	for (jp = jobmru; jp != NULL; jp = jp->next)
		if (jp->used && jp != nj && jp->state == JOBSTOPPED)
			return (jp);
	/* Otherwise the most recently used job that isn't `nj' */
	for (jp = jobmru; jp != NULL; jp = jp->next)
		if (jp->used && jp != nj)
			return (jp);

	return (NULL);
}

#endif

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

pid_t
forkshell(struct job *jp, union node *n, int mode)
{
	pid_t pid;
	pid_t pgrp;

	TRACE(("forkshell(%%%d, %p, %d) called\n", jp - jobtab, (void *)n,
	    mode));
	INTOFF;
	if (mode == FORK_BG)
		checkzombies();
	flushall();
	pid = fork();
	if (pid == -1) {
		TRACE(("Fork failed, errno=%d\n", errno));
		INTON;
		error("Cannot fork: %s", strerror(errno));
	}
	if (pid == 0) {
		struct job *p;
		int wasroot;
		int i;

		TRACE(("Child shell %d\n", (int)getpid()));
		wasroot = rootshell;
		rootshell = 0;
		handler = &main_handler;
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
				if (tcsetpgrp(ttyfd, pgrp) < 0)
					error("tcsetpgrp failed, errno=%d", errno);
			}
			setsignal(SIGTSTP);
			setsignal(SIGTTOU);
		} else if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open(_PATH_DEVNULL, O_RDONLY) != 0)
					error("Can't open %s: %s",
					    _PATH_DEVNULL, strerror(errno));
			}
		}
#else
		if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open(_PATH_DEVNULL, O_RDONLY) != 0)
					error("Can't open %s: %s",
					    _PATH_DEVNULL, strerror(errno));
			}
		}
#endif
		INTOFF;
		for (i = njobs, p = jobtab ; --i >= 0 ; p++)
			if (p->used)
				freejob(p);
		INTON;
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
		jp->foreground = mode == FORK_FG;
#if JOBS
		setcurjob(jp);
#endif
	}
	INTON;
	TRACE(("In parent shell:  child = %d\n", (int)pid));
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
 * foreground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 */

int
waitforjob(struct job *jp, int *origstatus)
{
#if JOBS
	pid_t mypgrp = getpgrp();
	int propagate_int = jp->jobctl && jp->foreground;
#endif
	int status;
	int st;

	INTOFF;
	TRACE(("waitforjob(%%%d) called\n", jp - jobtab + 1));
	while (jp->state == 0)
		if (dowait(1, jp) == -1)
			dotrap();
#if JOBS
	if (jp->jobctl) {
		if (tcsetpgrp(ttyfd, mypgrp) < 0)
			error("tcsetpgrp failed, errno=%d\n", errno);
	}
	if (jp->state == JOBSTOPPED)
		setcurjob(jp);
#endif
	status = jp->ps[jp->nprocs - 1].status;
	if (origstatus != NULL)
		*origstatus = status;
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
	if (int_pending()) {
		if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
			kill(getpid(), SIGINT);
		else
			CLEAR_PENDING_INT;
	}
#if JOBS
	else if (rootshell && iflag && propagate_int &&
			WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
		kill(getpid(), SIGINT);
#endif
	INTON;
	return st;
}



/*
 * Wait for a process to terminate.
 */

STATIC pid_t
dowait(int block, struct job *job)
{
	pid_t pid;
	int status;
	struct procstat *sp;
	struct job *jp;
	struct job *thisjob;
	int done;
	int stopped;
	int sig;
	int i;

	in_dowait++;
	TRACE(("dowait(%d) called\n", block));
	do {
		pid = waitproc(block, &status);
		TRACE(("wait returns %d, status=%d\n", (int)pid, status));
	} while ((pid == -1 && errno == EINTR && breakwaitcmd == 0) ||
		 (pid > 0 && WIFSTOPPED(status) && !iflag));
	in_dowait--;
	if (pid == -1 && errno == ECHILD && job != NULL)
		job->state = JOBDONE;
	if (breakwaitcmd != 0) {
		breakwaitcmd = 0;
		if (pid <= 0)
			return -1;
	}
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
						   (int)pid, sp->status,
						   status));
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
					if (done)
						deljob(jp);
#endif
				}
			}
		}
	}
	INTON;
	if (! rootshell || ! iflag || (job && thisjob == job)) {
#if JOBS
		if (WIFSTOPPED(status))
			sig = WSTOPSIG(status);
		else
#endif
		{
			if (WIFEXITED(status))
				sig = 0;
			else
				sig = WTERMSIG(status);
		}
		if (sig != 0 && sig != SIGINT && sig != SIGPIPE) {
			if (!mflag ||
			    (thisjob->foreground && !WIFSTOPPED(status))) {
				i = WTERMSIG(status);
				if ((i & 0x7F) < sys_nsig && sys_siglist[i & 0x7F])
					out1str(sys_siglist[i & 0x7F]);
				else
					out1fmt("Signal %d", i & 0x7F);
				if (WCOREDUMP(status))
					out1str(" (core dumped)");
				out1c('\n');
			} else
				showjob(thisjob, pid, SHOWJOBS_DEFAULT);
		}
	} else {
		TRACE(("Not printing status, rootshell=%d, job=%p\n", rootshell, job));
		if (thisjob)
			thisjob->changed = 1;
	}
	return pid;
}



/*
 * Do a wait system call.  If job control is compiled in, we accept
 * stopped processes.  If block is zero, we return a value of zero
 * rather than blocking.
 */
STATIC pid_t
waitproc(int block, int *status)
{
	int flags;

#if JOBS
	flags = WUNTRACED;
#else
	flags = 0;
#endif
	if (block == 0)
		flags |= WNOHANG;
	return wait3(status, flags, (struct rusage *)NULL);
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
int job_warning = 0;
int
stoppedjobs(void)
{
	int jobno;
	struct job *jp;

	if (job_warning)
		return (0);
	for (jobno = 1, jp = jobtab; jobno <= njobs; jobno++, jp++) {
		if (jp->used == 0)
			continue;
		if (jp->state == JOBSTOPPED) {
			out2fmt_flush("You have stopped jobs.\n");
			job_warning = 2;
			return (1);
		}
	}

	return (0);
}


STATIC void
checkzombies(void)
{
	while (njobs > 0 && dowait(0, NULL) > 0)
		;
}


/*
 * Return a string identifying a command (to be printed by the
 * jobs command.
 */

STATIC char *cmdnextc;
STATIC int cmdnleft;
#define MAXCMDTEXT	200

char *
commandtext(union node *n)
{
	char *name;

	cmdnextc = name = ckmalloc(MAXCMDTEXT);
	cmdnleft = MAXCMDTEXT - 4;
	cmdtxt(n);
	*cmdnextc = '\0';
	return name;
}


STATIC void
cmdtxt(union node *n)
{
	union node *np;
	struct nodelist *lp;
	const char *p;
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
	case NCLOBBER:
		p = ">|"; i = 1; goto redir;
	case NFROM:
		p = "<";  i = 0;  goto redir;
	case NFROMTO:
		p = "<>";  i = 0;  goto redir;
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
			if (n->ndup.dupfd >= 0)
				s[0] = n->ndup.dupfd + '0';
			else
				s[0] = '-';
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
cmdputs(const char *s)
{
	const char *p;
	char *q;
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
