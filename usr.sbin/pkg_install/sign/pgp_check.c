/* $OpenBSD: pgp_check.c,v 1.2 1999/10/07 16:30:32 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	"/dev/null"
#endif

/* transform current process into pgp signature checker -u userid <fd */
static void 
pgpcheck(fd, userid, envp) 
	int fd;
	const char *userid;
	char *envp[];
{
	int fdnull;
	pchar argv[6];
	int argc = 0;

	argv[argc++] = PGP;
	argv[argc++] = "+batchmode";
	argv[argc++] = "-f";

	if (userid) {
		argv[argc++] = "-u";
		argv[argc++] = (char *)userid;
	}
	argv[argc++] = NULL;

	assert(argc <= sizeof argv / sizeof(pchar));

	fdnull = open(_PATH_DEVNULL, O_RDWR);
	if (fdnull == -1 ||
	    dup2(fd, fileno(stdin)) == -1 || 
	    dup2(fdnull, fileno(stdout)) == -1 ||
		 close(fdnull) == -1 || close(fd) == -1 ||
	    execve(PGP, argv, envp)  == -1)
		 perror("launching pgp");
		exit(errno);
}

struct pgp_checker {
	pid_t id;
	int fdout;
	int status;
#ifdef DEBUG_DUMP
	FILE *out;
#endif
};

void *
new_pgp_checker(h, sign, userid, envp, filename)
	struct mygzip_header *h;
	struct signature *sign;
	const char *userid;	
	char *envp[];
	/*@observer@*/const char *filename;
{
	struct pgp_checker *n;
	int topgpcheck[2];

	assert(sign->type == TAG_PGP);
	n = malloc(sizeof *n);

	{
		struct stat sbuf;

		if (stat(PGP, &sbuf) == -1) {
			warnx("%s does not exist", PGP);
			return NULL;
		}
	}
	if (n == NULL) {
		warnx("Can't allocate pgp_checker");
		return NULL;
	}

	if (pipe(topgpcheck) == -1) {
		warn("Pgp checker pipe");
		free(n);
		return NULL;
	}
	switch(n->id = fork()) {
	case -1:
		warn("Pgp checker process");
		free(n);
		return NULL;
	case 0:
		if (close(topgpcheck[1]) == -1)
			exit(errno);
		pgpcheck(topgpcheck[0], userid, envp);
		/*@notreached@*/
		break;
	default:
		(void)close(topgpcheck[0]);
		break;
	}
	n->fdout = topgpcheck[1];
		/* so that subsequent fork() won't duplicate it inadvertently */
	(void)fcntl(n->fdout, F_SETFD, FD_CLOEXEC);	
#ifdef DEBUG_DUMP
	n->out = fopen("compare", "w");
#endif
	n->status = PKG_GOODSIG;

	pgp_add(n, sign->data, sign->length);
	if (gzip_copy_header(h, sign->next, pgp_add, n) == 0) {
		warnx("Unexpected header in %s", filename);
		n->status = PKG_SIGERROR;
	}
	return n;
}
	
void 
pgp_add(arg, buffer, length)
	void *arg;
	const char *buffer;
	size_t length;
{
	struct pgp_checker *n = arg;

	if (n->status == PKG_GOODSIG) {
#ifdef DEBUG_DUMP
	fwrite(buffer, 1, length, n->out);
#endif
		while (length > 0) {
			ssize_t l = write(n->fdout, buffer, length);
			if (l == -1) {
				n->status = PKG_SIGERROR;
				break;
			}
			length -= l;
			buffer += l;
		}
	}
}

int
pgp_sign_ok(arg)
	void *arg;
{
	struct pgp_checker *n = arg;
	int status = n->status;

#ifdef DEBUG_DUMP
	fclose(n->out);
#endif
	if (close(n->fdout) != 0)
		status = PKG_SIGERROR;
	if (reap(n->id) != 0)
		status = PKG_BADSIG;
	free(n);
	return status;
}
