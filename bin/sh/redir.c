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
#if 0
static char sccsid[] = "@(#)redir.c	8.2 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * Code for dealing with input/output redirection.
 */

#include "shell.h"
#include "nodes.h"
#include "jobs.h"
#include "expand.h"
#include "redir.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "options.h"


#define EMPTY -2		/* marks an unused slot in redirtab */
#define PIPESIZE 4096		/* amount of buffering in a pipe */


MKINIT
struct redirtab {
	struct redirtab *next;
	int renamed[10];
};


MKINIT struct redirtab *redirlist;

/*
 * We keep track of whether or not fd0 has been redirected.  This is for
 * background commands, where we want to redirect fd0 to /dev/null only
 * if it hasn't already been redirected.
*/
int fd0_redirected = 0;

STATIC void openredirect(union node *, char[10 ]);
STATIC int openhere(union node *);


/*
 * Process a list of redirection commands.  If the REDIR_PUSH flag is set,
 * old file descriptors are stashed away so that the redirection can be
 * undone by calling popredir.  If the REDIR_BACKQ flag is set, then the
 * standard output, and the standard error if it becomes a duplicate of
 * stdout, is saved in memory.
 */

void
redirect(union node *redir, int flags)
{
	union node *n;
	struct redirtab *sv = NULL;
	int i;
	int fd;
	int try;
	char memory[10];	/* file descriptors to write to memory */

	for (i = 10 ; --i >= 0 ; )
		memory[i] = 0;
	memory[1] = flags & REDIR_BACKQ;
	if (flags & REDIR_PUSH) {
		sv = ckmalloc(sizeof (struct redirtab));
		for (i = 0 ; i < 10 ; i++)
			sv->renamed[i] = EMPTY;
		sv->next = redirlist;
		redirlist = sv;
	}
	for (n = redir ; n ; n = n->nfile.next) {
		fd = n->nfile.fd;
		try = 0;
		if ((n->nfile.type == NTOFD || n->nfile.type == NFROMFD) &&
		    n->ndup.dupfd == fd)
			continue; /* redirect from/to same file descriptor */

		if ((flags & REDIR_PUSH) && sv->renamed[fd] == EMPTY) {
			INTOFF;
again:
			if ((i = fcntl(fd, F_DUPFD, 10)) == -1) {
				switch (errno) {
				case EBADF:
					if (!try) {
						openredirect(n, memory);
						try++;
						goto again;
					}
					/* FALLTHROUGH*/
				default:
					INTON;
					error("%d: %s", fd, strerror(errno));
					break;
				}
			}
			if (!try) {
				sv->renamed[fd] = i;
			}
			INTON;
		}
		if (fd == 0)
			fd0_redirected++;
		if (!try)
			openredirect(n, memory);
	}
	if (memory[1])
		out1 = &memout;
	if (memory[2])
		out2 = &memout;
}


STATIC void
openredirect(union node *redir, char memory[10])
{
	struct stat sb;
	int fd = redir->nfile.fd;
	char *fname;
	int f;

	/*
	 * We suppress interrupts so that we won't leave open file
	 * descriptors around.  This may not be such a good idea because
	 * an open of a device or a fifo can block indefinitely.
	 */
	INTOFF;
	memory[fd] = 0;
	switch (redir->nfile.type) {
	case NFROM:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_RDONLY)) < 0)
			error("cannot open %s: %s", fname, strerror(errno));
movefd:
		if (f != fd) {
			close(fd);
			copyfd(f, fd);
			close(f);
		}
		break;
	case NFROMTO:
		fname = redir->nfile.expfname;
#ifdef O_CREAT
		if ((f = open(fname, O_RDWR|O_CREAT, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
#else
		if ((f = open(fname, O_RDWR, 0666)) < 0) {
			if (errno != ENOENT)
				error("cannot create %s: %s", fname, strerror(errno));
			else if ((f = creat(fname, 0666)) < 0)
				error("cannot create %s: %s", fname, strerror(errno));
			else {
				close(f);
				if ((f = open(fname, O_RDWR)) < 0) {
					error("cannot create %s: %s", fname, strerror(errno));
					remove(fname);
				}
			}
		}
#endif
		goto movefd;
	case NTO:
		fname = redir->nfile.expfname;
		if (Cflag && stat(fname, &sb) != -1 && S_ISREG(sb.st_mode))
			error("cannot create %s: %s", fname,
			    strerror(EEXIST));
#ifdef O_CREAT
		if ((f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
#else
		if ((f = creat(fname, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
#endif
		goto movefd;
	case NCLOBBER:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
		goto movefd;
	case NAPPEND:
		fname = redir->nfile.expfname;
#ifdef O_APPEND
		if ((f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
#else
		if ((f = open(fname, O_WRONLY)) < 0
		 && (f = creat(fname, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
		lseek(f, (off_t)0, 2);
#endif
		goto movefd;
	case NTOFD:
	case NFROMFD:
		if (redir->ndup.dupfd >= 0) {	/* if not ">&-" */
			if (memory[redir->ndup.dupfd])
				memory[fd] = 1;
			else {
				close(fd);
				copyfd(redir->ndup.dupfd, fd);
			}
		} else
			close(fd);
		break;
	case NHERE:
	case NXHERE:
		f = openhere(redir);
		goto movefd;
	default:
		abort();
	}
	INTON;
}


/*
 * Handle here documents.  Normally we fork off a process to write the
 * data to a pipe.  If the document is short, we can stuff the data in
 * the pipe without forking.
 */

STATIC int
openhere(union node *redir)
{
	int pip[2];
	int len = 0;

	if (pipe(pip) < 0)
		error("Pipe call failed: %s", strerror(errno));
	if (redir->type == NHERE) {
		len = strlen(redir->nhere.doc->narg.text);
		if (len <= PIPESIZE) {
			xwrite(pip[1], redir->nhere.doc->narg.text, len);
			goto out;
		}
	}
	if (forkshell((struct job *)NULL, (union node *)NULL, FORK_NOJOB) == 0) {
		close(pip[0]);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
#ifdef SIGTSTP
		signal(SIGTSTP, SIG_IGN);
#endif
		signal(SIGPIPE, SIG_DFL);
		if (redir->type == NHERE)
			xwrite(pip[1], redir->nhere.doc->narg.text, len);
		else
			expandhere(redir->nhere.doc, pip[1]);
		_exit(0);
	}
out:
	close(pip[1]);
	return pip[0];
}



/*
 * Undo the effects of the last redirection.
 */

void
popredir(void)
{
	struct redirtab *rp = redirlist;
	int i;

	for (i = 0 ; i < 10 ; i++) {
		if (rp->renamed[i] != EMPTY) {
                        if (i == 0)
                                fd0_redirected--;
			close(i);
			if (rp->renamed[i] >= 0) {
				copyfd(rp->renamed[i], i);
				close(rp->renamed[i]);
			}
		}
	}
	INTOFF;
	redirlist = rp->next;
	ckfree(rp);
	INTON;
}

/*
 * Undo all redirections.  Called on error or interrupt.
 */

#ifdef mkinit

INCLUDE "redir.h"

RESET {
	while (redirlist)
		popredir();
}

SHELLPROC {
	clearredir();
}

#endif

/* Return true if fd 0 has already been redirected at least once.  */
int
fd0_redirected_p(void)
{
        return fd0_redirected != 0;
}

/*
 * Discard all saved file descriptors.
 */

void
clearredir(void)
{
	struct redirtab *rp;
	int i;

	for (rp = redirlist ; rp ; rp = rp->next) {
		for (i = 0 ; i < 10 ; i++) {
			if (rp->renamed[i] >= 0) {
				close(rp->renamed[i]);
			}
			rp->renamed[i] = EMPTY;
		}
	}
}



/*
 * Copy a file descriptor to be >= to.  Returns -1
 * if the source file descriptor is closed, EMPTY if there are no unused
 * file descriptors left.
 */

int
copyfd(int from, int to)
{
	int newfd;

	newfd = fcntl(from, F_DUPFD, to);
	if (newfd < 0) {
		if (errno == EMFILE)
			return EMPTY;
		else
			error("%d: %s", from, strerror(errno));
	}
	return newfd;
}
