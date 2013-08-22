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
#define CLOSED -1		/* fd was not open before redir */


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
static int fd0_redirected = 0;

static void openredirect(union node *, char[10 ]);
static int openhere(union node *);


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
		if ((n->nfile.type == NTOFD || n->nfile.type == NFROMFD) &&
		    n->ndup.dupfd == fd)
			continue; /* redirect from/to same file descriptor */

		if ((flags & REDIR_PUSH) && sv->renamed[fd] == EMPTY) {
			INTOFF;
			if ((i = fcntl(fd, F_DUPFD, 10)) == -1) {
				switch (errno) {
				case EBADF:
					i = CLOSED;
					break;
				default:
					INTON;
					error("%d: %s", fd, strerror(errno));
					break;
				}
			} else
				(void)fcntl(i, F_SETFD, FD_CLOEXEC);
			sv->renamed[fd] = i;
			INTON;
		}
		if (fd == 0)
			fd0_redirected++;
		openredirect(n, memory);
	}
	if (memory[1])
		out1 = &memout;
	if (memory[2])
		out2 = &memout;
}


static void
openredirect(union node *redir, char memory[10])
{
	struct stat sb;
	int fd = redir->nfile.fd;
	char *fname;
	int f;
	int e;

	/*
	 * We suppress interrupts so that we won't leave open file
	 * descriptors around.  Because the signal handler remains
	 * installed and we do not use system call restart, interrupts
	 * will still abort blocking opens such as fifos (they will fail
	 * with EINTR). There is, however, a race condition if an interrupt
	 * arrives after INTOFF and before open blocks.
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
			if (dup2(f, fd) == -1) {
				e = errno;
				close(f);
				error("%d: %s", fd, strerror(e));
			}
			close(f);
		}
		break;
	case NFROMTO:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_RDWR|O_CREAT, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
		goto movefd;
	case NTO:
		if (Cflag) {
			fname = redir->nfile.expfname;
			if (stat(fname, &sb) == -1) {
				if ((f = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0666)) < 0)
					error("cannot create %s: %s", fname, strerror(errno));
			} else if (!S_ISREG(sb.st_mode)) {
				if ((f = open(fname, O_WRONLY, 0666)) < 0)
					error("cannot create %s: %s", fname, strerror(errno));
				if (fstat(f, &sb) != -1 && S_ISREG(sb.st_mode)) {
					close(f);
					error("cannot create %s: %s", fname,
					    strerror(EEXIST));
				}
			} else
				error("cannot create %s: %s", fname,
				    strerror(EEXIST));
			goto movefd;
		}
		/* FALLTHROUGH */
	case NCLOBBER:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
		goto movefd;
	case NAPPEND:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666)) < 0)
			error("cannot create %s: %s", fname, strerror(errno));
		goto movefd;
	case NTOFD:
	case NFROMFD:
		if (redir->ndup.dupfd >= 0) {	/* if not ">&-" */
			if (memory[redir->ndup.dupfd])
				memory[fd] = 1;
			else {
				if (dup2(redir->ndup.dupfd, fd) < 0)
					error("%d: %s", redir->ndup.dupfd,
							strerror(errno));
			}
		} else {
			close(fd);
		}
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

static int
openhere(union node *redir)
{
	char *p;
	int pip[2];
	size_t len = 0;
	int flags;
	ssize_t written = 0;

	if (pipe(pip) < 0)
		error("Pipe call failed: %s", strerror(errno));

	if (redir->type == NXHERE)
		p = redir->nhere.expdoc;
	else
		p = redir->nhere.doc->narg.text;
	len = strlen(p);
	if (len == 0)
		goto out;
	flags = fcntl(pip[1], F_GETFL, 0);
	if (flags != -1 && fcntl(pip[1], F_SETFL, flags | O_NONBLOCK) != -1) {
		written = write(pip[1], p, len);
		if (written < 0)
			written = 0;
		if ((size_t)written == len)
			goto out;
		fcntl(pip[1], F_SETFL, flags);
	}

	if (forkshell((struct job *)NULL, (union node *)NULL, FORK_NOJOB) == 0) {
		close(pip[0]);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGPIPE, SIG_DFL);
		xwrite(pip[1], p + written, len - written);
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
			if (rp->renamed[i] >= 0) {
				dup2(rp->renamed[i], i);
				close(rp->renamed[i]);
			} else {
				close(i);
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
