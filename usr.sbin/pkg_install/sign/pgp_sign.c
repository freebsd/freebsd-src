/* $OpenBSD: pgp_sign.c,v 1.1 1999/10/04 21:46:29 espie Exp $ */
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

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <assert.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

static void 
pgpsign(fdin, fdout, userid, envp) 
	int fdin, fdout;
	const char *userid;
	char *envp[];
{
	pchar argv[10];
	int argc = 0;

	argv[argc++] = PGP;
	argv[argc++] = "+batchmode";
	argv[argc++] = "+compress=off";
	argv[argc++] = "-f";
	argv[argc++] = "-s";
	argv[argc++] = "-zAthlon";

	if (userid) {
		argv[argc++] = "-u";
		argv[argc++] = (char *)userid;
	}
	argv[argc++] = NULL;
	assert(argc <= sizeof argv / sizeof(pchar));

	if (dup2(fdin, fileno(stdin)) == -1 || 
	    dup2(fdout, fileno(stdout)) == -1 ||
	    execve(PGP, argv, envp)  == -1)
		exit(errno);
}

static struct signature *
new_pgpsignature(old)
	struct signature *old;
{
	struct signature *n;

	n = malloc(sizeof(*n));
	if (n != NULL) {
		n->data = malloc(MAXPGPSIGNSIZE);
		if (n->data == NULL) {
			free(n);
			return NULL;
		}
		n->length = 0;
		n->next = old;
		n->type = TAG_PGP;
		memcpy(n->tag, pgptag, sizeof pgptag);
	}
	return n;
}

int
retrieve_pgp_signature(filename, sign, userid, envp)
	const char *filename; 
	struct signature **sign;
	const char *userid;
	char *envp[];
{
	int topgp[2], frompgp[2];
	pid_t pgpid;
	struct mygzip_header h;
	int success;

	FILE *orig, *dest, *signin;
	struct signature *old;

	orig = fopen(filename, "r");
	if (orig == NULL)
		return 0;
	if (gzip_read_header(orig, &h, &old) == GZIP_NOT_GZIP) {
		warnx("File %s is not a gzip file\n", filename);
		fclose(orig);
		return 0;
	}

	if (pipe(topgp) == -1) {
		fclose(orig);
		return 0;
	}
	if (pipe(frompgp) == -1) {
		fclose(orig);
		(void)close(topgp[0]);
		(void)close(topgp[1]);
		return 0;
	}
	switch(pgpid = fork()) {
	case 0:
		(void)close(topgp[1]);
		(void)close(frompgp[0]);
		pgpsign(topgp[0], frompgp[1], userid, envp);
		/*NOT REACHED */
	case -1:
		(void)close(topgp[0]);
		(void)close(topgp[1]);
		(void)close(frompgp[0]);
		(void)close(frompgp[1]);
		fclose(orig);
		return 0;
	default:
		(void)close(topgp[0]);
		(void)close(frompgp[1]);
	}

	dest = fdopen(topgp[1], "w");
	if (dest == NULL) {
		(void)close(topgp[1]);
		(void)close(frompgp[0]);
		(void)reap(pgpid);
		return 0;
	}

	success = 1;
	if (gzip_write_header(dest, &h, old) == 0)
		success = 0;
	else {
		int c;

		while ((c = fgetc(orig)) != EOF && fputc(c, dest) != EOF)
			;
		if (ferror(dest))
			success = 0;
	}
	if (fclose(dest) != 0)
		success = 0;

	if (fclose(orig) != 0)
		success = 0;

	signin = fdopen(frompgp[0], "r");
	if (signin == NULL) {
		(void)close(frompgp[0]);
	} else {
		enum { NONE, FIRST, DONE, COPY} magic = NONE;
		int c;
#ifdef DEBUG_DUMP
		FILE *out = fopen("dump", "w");
#endif

		if ((*sign = new_pgpsignature(old)) == NULL) 
			success = 0;
		else {
			while ((c = fgetc(signin)) != EOF && magic != DONE && 
				(*sign)->length < MAXPGPSIGNSIZE) {
				switch(magic) {
				case NONE:
					(*sign)->data[(*sign)->length++] = c;
					if ((unsigned char)c == (unsigned char)GZIP_MAGIC0)
						magic = FIRST;
					break;
				case FIRST:
					(*sign)->data[(*sign)->length++] = c;
					if ((unsigned char)c == (unsigned char)GZIP_MAGIC1)
#ifdef DEBUG_DUMP
						magic = COPY;
#else
						magic = DONE;
#endif
					else if ((unsigned char)c != (unsigned char)GZIP_MAGIC0)
						magic = NONE;
					break;
				case DONE:
				case COPY:
					break;
				}
#ifdef DEBUG_DUMP
				fputc(c, out);
#endif
			}
			if ((*sign)->length == MAXPGPSIGNSIZE)
				success = 0;
			(*sign)->length -= 2;
			sign_fill_tag(*sign);
		}
		fclose(signin);
#ifdef DEBUG_DUMP
		fclose(out);
#endif
		reap(pgpid);
	}
	return success;
}

void
handle_pgp_passphrase()
{
	pid_t pid;
	int fd[2];
	char *p;

printf("Short-circuiting %s\n", __func__);
return;

		/* Retrieve the pgp passphrase */
	p = getpass("Enter passphrase:");

		/*
		 * Somewhat kludgy code to get the passphrase to pgp, see 
		 * pgp documentation for the gore
		 */
	if (pipe(fd) != 0)	{
		perror("pkg_sign");
		exit(EXIT_FAILURE);
	}
	switch(pid = fork()) {
	case -1:
		perror("pkg_sign");
		exit(EXIT_FAILURE);
	case 0:
		{
			(void)close(fd[0]);
				/*
				 * The child fills the pipe with copies of the passphrase.
				 * Expect violent death when father exits.
				 */
			printf("Child process %d stuffing passphrase in pipe:\n", getpid());
			for(;;) {
				char c = '\n';
				(void)write(fd[1], p, strlen(p));
				(void)write(fd[1], &c, 1);
				putchar('.'); fflush(stdout);
			}
		}
	default:
		{
			char buf[10];

			sleep(1);
			(void)close(fd[1]);
			(void)sprintf(buf, "%d", fd[0]);
			(void)setenv("PGPPASSFD", buf, 1);
			printf("Parent process PGPPASSFD=%d.\n", fd[0]);
		}
	}
}

