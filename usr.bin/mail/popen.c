/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)popen.c	5.16 (Berkeley) 4/1/91";
#endif /* not lint */

#include "rcv.h"
#include <sys/signal.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1
static int *pid;

struct fp {
	FILE *fp;
	int pipe;
	struct fp *link;
};
static struct fp *fp_head;

FILE *
Fopen(file, mode)
	char *file, *mode;
{
	FILE *fp;

	if ((fp = fopen(file, mode)) != NULL)
		register_file(fp, 0);
	return fp;
}

FILE *
Fdopen(fd, mode)
	char *mode;
{
	FILE *fp;

	if ((fp = fdopen(fd, mode)) != NULL)
		register_file(fp, 0);
	return fp;
}

Fclose(fp)
	FILE *fp;
{
	unregister_file(fp);
	return fclose(fp);
}

FILE *
Popen(cmd, mode)
	char *cmd;
	char *mode;
{
	int p[2];
	int myside, hisside, fd0, fd1;
	FILE *fp;

	if (pid == 0)
		pid = (int *) malloc((unsigned) sizeof (int) * getdtablesize());
	if (pipe(p) < 0)
		return NULL;
	if (*mode == 'r') {
		myside = p[READ];
		fd0 = -1;
		hisside = fd1 = p[WRITE];
	} else {
		myside = p[WRITE];
		hisside = fd0 = p[READ];
		fd1 = -1;
	}
	if ((pid[myside] = start_command(cmd, 0, fd0, fd1, NOSTR)) < 0) {
		close(p[READ]);
		close(p[WRITE]);
		return NULL;
	}
	(void) close(hisside);
	if ((fp = fdopen(myside, mode)) != NULL)
		register_file(fp, 1);
	return fp;
}

Pclose(ptr)
	FILE *ptr;
{
	int i;
	int omask;

	i = fileno(ptr);
	unregister_file(ptr);
	(void) fclose(ptr);
	omask = sigblock(sigmask(SIGINT)|sigmask(SIGHUP));
	i = wait_child(pid[i]);
	sigsetmask(omask);
	return i;
}

close_all_files()
{

	while (fp_head)
		if (fp_head->pipe)
			(void) Pclose(fp_head->fp);
		else
			(void) Fclose(fp_head->fp);
}

register_file(fp, pipe)
	FILE *fp;
{
	struct fp *fpp;

	if ((fpp = (struct fp *) malloc(sizeof *fpp)) == NULL)
		panic("Out of memory");
	fpp->fp = fp;
	fpp->pipe = pipe;
	fpp->link = fp_head;
	fp_head = fpp;
}

unregister_file(fp)
	FILE *fp;
{
	struct fp **pp, *p;

	for (pp = &fp_head; p = *pp; pp = &p->link)
		if (p->fp == fp) {
			*pp = p->link;
			free((char *) p);
			return;
		}
	/* XXX
	 * Ignore this for now; there may still be uncaught
	 * duplicate closes.
	panic("Invalid file pointer");
	*/
}

/*
 * Run a command without a shell, with optional arguments and splicing
 * of stdin and stdout.  The command name can be a sequence of words.
 * Signals must be handled by the caller.
 * "Mask" contains the signals to ignore in the new process.
 * SIGINT is enabled unless it's in the mask.
 */
/*VARARGS4*/
run_command(cmd, mask, infd, outfd, a0, a1, a2)
	char *cmd;
	int mask, infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = start_command(cmd, mask, infd, outfd, a0, a1, a2)) < 0)
		return -1;
	return wait_command(pid);
}

/*VARARGS4*/
start_command(cmd, mask, infd, outfd, a0, a1, a2)
	char *cmd;
	int mask, infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = vfork()) < 0) {
		perror("fork");
		return -1;
	}
	if (pid == 0) {
		char *argv[100];
		int i = getrawlist(cmd, argv, sizeof argv / sizeof *argv);

		if ((argv[i++] = a0) != NOSTR &&
		    (argv[i++] = a1) != NOSTR &&
		    (argv[i++] = a2) != NOSTR)
			argv[i] = NOSTR;
		prepare_child(mask, infd, outfd);
		execvp(argv[0], argv);
		perror(argv[0]);
		_exit(1);
	}
	return pid;
}

prepare_child(mask, infd, outfd)
	int mask, infd, outfd;
{
	int i;

	if (infd >= 0)
		dup2(infd, 0);
	if (outfd >= 0)
		dup2(outfd, 1);
	for (i = getdtablesize(); --i > 2;)
		close(i);
	for (i = 1; i <= NSIG; i++)
		if (mask & sigmask(i))
			(void) signal(i, SIG_IGN);
	if ((mask & sigmask(SIGINT)) == 0)
		(void) signal(SIGINT, SIG_DFL);
	(void) sigsetmask(0);
}

wait_command(pid)
	int pid;
{

	if (wait_child(pid) < 0) {
		printf("Fatal error in process.\n");
		return -1;
	}
	return 0;
}

struct child {
	int pid;
	char done;
	char free;
	union wait status;
	struct child *link;
};
static struct child *child;

struct child *
findchild(pid)
	int pid;
{
	register struct child **cpp;

	for (cpp = &child; *cpp != NULL && (*cpp)->pid != pid;
	     cpp = &(*cpp)->link)
			;
	if (*cpp == NULL) {
		*cpp = (struct child *) malloc(sizeof (struct child));
		(*cpp)->pid = pid;
		(*cpp)->done = (*cpp)->free = 0;
		(*cpp)->link = NULL;
	}
	return *cpp;
}

delchild(cp)
	register struct child *cp;
{
	register struct child **cpp;

	for (cpp = &child; *cpp != cp; cpp = &(*cpp)->link)
		;
	*cpp = cp->link;
	free((char *) cp);
}

void
sigchild()
{
	int pid;
	union wait status;
	register struct child *cp;

	while ((pid =
	    wait3((int *)&status, WNOHANG, (struct rusage *)0)) > 0) {
		cp = findchild(pid);
		if (cp->free)
			delchild(cp);
		else {
			cp->done = 1;
			cp->status = status;
		}
	}
}

union wait wait_status;

/*
 * Wait for a specific child to die.
 */
wait_child(pid)
	int pid;
{
	int mask = sigblock(sigmask(SIGCHLD));
	register struct child *cp = findchild(pid);

	while (!cp->done)
		sigpause(mask);
	wait_status = cp->status;
	delchild(cp);
	sigsetmask(mask);
	return wait_status.w_status ? -1 : 0;
}

/*
 * Mark a child as don't care.
 */
free_child(pid)
	int pid;
{
	int mask = sigblock(sigmask(SIGCHLD));
	register struct child *cp = findchild(pid);

	if (cp->done)
		delchild(cp);
	else
		cp->free = 1;
	sigsetmask(mask);
}
