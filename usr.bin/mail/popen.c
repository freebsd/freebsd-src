/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)popen.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "rcv.h"
#include <sys/wait.h>
#include <fcntl.h>
#include "extern.h"

#define READ 0
#define WRITE 1

struct fp {
	FILE	*fp;
	int	pipe;
	int	pid;
	struct	fp *link;
};
static struct fp *fp_head;

struct child {
	int	pid;
	char	done;
	char	free;
	int	status;
	struct	child *link;
};
static struct child *child;
static struct child *findchild(int);
static void delchild(struct child *);
static int file_pid(FILE *);

FILE *
Fopen(path, mode)
	const char *path, *mode;
{
	FILE *fp;

	if ((fp = fopen(path, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return (fp);
}

FILE *
Fdopen(fd, mode)
	int fd;
	const char *mode;
{
	FILE *fp;

	if ((fp = fdopen(fd, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return (fp);
}

int
Fclose(fp)
	FILE *fp;
{
	unregister_file(fp);
	return (fclose(fp));
}

FILE *
Popen(cmd, mode)
	char *cmd;
	const char *mode;
{
	int p[2];
	int myside, hisside, fd0, fd1;
	int pid;
	sigset_t nset;
	FILE *fp;

	if (pipe(p) < 0)
		return (NULL);
	(void)fcntl(p[READ], F_SETFD, 1);
	(void)fcntl(p[WRITE], F_SETFD, 1);
	if (*mode == 'r') {
		myside = p[READ];
		fd0 = -1;
		hisside = fd1 = p[WRITE];
	} else {
		myside = p[WRITE];
		hisside = fd0 = p[READ];
		fd1 = -1;
	}
	(void)sigemptyset(&nset);
	if ((pid = start_command(cmd, &nset, fd0, fd1, NULL, NULL, NULL)) < 0) {
		(void)close(p[READ]);
		(void)close(p[WRITE]);
		return (NULL);
	}
	(void)close(hisside);
	if ((fp = fdopen(myside, mode)) != NULL)
		register_file(fp, 1, pid);
	return (fp);
}

int
Pclose(ptr)
	FILE *ptr;
{
	int i;
	sigset_t nset, oset;

	i = file_pid(ptr);
	unregister_file(ptr);
	(void)fclose(ptr);
	(void)sigemptyset(&nset);
	(void)sigaddset(&nset, SIGINT);
	(void)sigaddset(&nset, SIGHUP);
	(void)sigprocmask(SIG_BLOCK, &nset, &oset);
	i = wait_child(i);
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	return (i);
}

void
close_all_files()
{

	while (fp_head != NULL)
		if (fp_head->pipe)
			(void)Pclose(fp_head->fp);
		else
			(void)Fclose(fp_head->fp);
}

void
register_file(fp, pipe, pid)
	FILE *fp;
	int pipe, pid;
{
	struct fp *fpp;

	if ((fpp = malloc(sizeof(*fpp))) == NULL)
		err(1, "Out of memory");
	fpp->fp = fp;
	fpp->pipe = pipe;
	fpp->pid = pid;
	fpp->link = fp_head;
	fp_head = fpp;
}

void
unregister_file(fp)
	FILE *fp;
{
	struct fp **pp, *p;

	for (pp = &fp_head; (p = *pp) != NULL; pp = &p->link)
		if (p->fp == fp) {
			*pp = p->link;
			(void)free(p);
			return;
		}
	errx(1, "Invalid file pointer");
	/*NOTREACHED*/
}

int
file_pid(fp)
	FILE *fp;
{
	struct fp *p;

	for (p = fp_head; p != NULL; p = p->link)
		if (p->fp == fp)
			return (p->pid);
	errx(1, "Invalid file pointer");
	/*NOTREACHED*/
}

/*
 * Run a command without a shell, with optional arguments and splicing
 * of stdin and stdout.  The command name can be a sequence of words.
 * Signals must be handled by the caller.
 * "Mask" contains the signals to ignore in the new process.
 * SIGINT is enabled unless it's in the mask.
 */
/*VARARGS4*/
int
run_command(cmd, mask, infd, outfd, a0, a1, a2)
	char *cmd;
	sigset_t *mask;
	int infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = start_command(cmd, mask, infd, outfd, a0, a1, a2)) < 0)
		return (-1);
	return (wait_command(pid));
}

/*VARARGS4*/
int
start_command(cmd, mask, infd, outfd, a0, a1, a2)
	char *cmd;
	sigset_t *mask;
	int infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = fork()) < 0) {
		warn("fork");
		return (-1);
	}
	if (pid == 0) {
		char *argv[100];
		int i = getrawlist(cmd, argv, sizeof(argv) / sizeof(*argv));

		if ((argv[i++] = a0) != NULL &&
		    (argv[i++] = a1) != NULL &&
		    (argv[i++] = a2) != NULL)
			argv[i] = NULL;
		prepare_child(mask, infd, outfd);
		execvp(argv[0], argv);
		warn("%s", argv[0]);
		_exit(1);
	}
	return (pid);
}

void
prepare_child(nset, infd, outfd)
	sigset_t *nset;
	int infd, outfd;
{
	int i;
	sigset_t eset;

	/*
	 * All file descriptors other than 0, 1, and 2 are supposed to be
	 * close-on-exec.
	 */
	if (infd >= 0)
		dup2(infd, 0);
	if (outfd >= 0)
		dup2(outfd, 1);
	for (i = 1; i < NSIG; i++)
		if (nset != NULL && sigismember(nset, i))
			(void)signal(i, SIG_IGN);
	if (nset == NULL || !sigismember(nset, SIGINT))
		(void)signal(SIGINT, SIG_DFL);
	(void)sigemptyset(&eset);
	(void)sigprocmask(SIG_SETMASK, &eset, NULL);
}

int
wait_command(pid)
	int pid;
{

	if (wait_child(pid) < 0) {
		printf("Fatal error in process.\n");
		return (-1);
	}
	return (0);
}

static struct child *
findchild(pid)
	int pid;
{
	struct child **cpp;

	for (cpp = &child; *cpp != NULL && (*cpp)->pid != pid;
	    cpp = &(*cpp)->link)
			;
	if (*cpp == NULL) {
		*cpp = malloc(sizeof(struct child));
		if (*cpp == NULL)
			err(1, "Out of memory");
		(*cpp)->pid = pid;
		(*cpp)->done = (*cpp)->free = 0;
		(*cpp)->link = NULL;
	}
	return (*cpp);
}

static void
delchild(cp)
	struct child *cp;
{
	struct child **cpp;

	for (cpp = &child; *cpp != cp; cpp = &(*cpp)->link)
		;
	*cpp = cp->link;
	(void)free(cp);
}

/*ARGSUSED*/
void
sigchild(signo)
	int signo;
{
	int pid;
	int status;
	struct child *cp;

	while ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
		cp = findchild(pid);
		if (cp->free)
			delchild(cp);
		else {
			cp->done = 1;
			cp->status = status;
		}
	}
}

int wait_status;

/*
 * Wait for a specific child to die.
 */
int
wait_child(pid)
	int pid;
{
	sigset_t nset, oset;
	struct child *cp = findchild(pid);

	(void)sigemptyset(&nset);
	(void)sigaddset(&nset, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &nset, &oset);	

	while (!cp->done)
		(void)sigsuspend(&oset);
	wait_status = cp->status;
	delchild(cp);
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	return ((WIFEXITED(wait_status) && WEXITSTATUS(wait_status)) ? -1 : 0);
}

/*
 * Mark a child as don't care.
 */
void
free_child(pid)
	int pid;
{
	sigset_t nset, oset;
	struct child *cp = findchild(pid);

	(void)sigemptyset(&nset);
	(void)sigaddset(&nset, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &nset, &oset);	

	if (cp->done)
		delchild(cp);
	else
		cp->free = 1;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
}
