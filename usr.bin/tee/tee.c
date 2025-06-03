/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct entry {
	int fd;
	const char *name;
	STAILQ_ENTRY(entry) entries;
};
static STAILQ_HEAD(, entry) head = STAILQ_HEAD_INITIALIZER(head);

static void add(int, const char *);
static int tee_open(const char *, int);
static void usage(void) __dead2;

int
main(int argc, char *argv[])
{
	char *bp, *buf;
	struct entry *p;
	int append, ch, exitval, fd, n, oflags, rval, wval;
#define	BSIZE (8 * 1024)

	append = 0;
	while ((ch = getopt(argc, argv, "ai")) != -1)
		switch((char)ch) {
		case 'a':
			append = 1;
			break;
		case 'i':
			(void)signal(SIGINT, SIG_IGN);
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if ((buf = malloc(BSIZE)) == NULL)
		err(1, "malloc");

	if (caph_limit_stdin() == -1 || caph_limit_stderr() == -1)
		err(EXIT_FAILURE, "unable to limit stdio");

	add(STDOUT_FILENO, "stdout");

	oflags = O_WRONLY | O_CREAT;
	if (append)
		oflags |= O_APPEND;
	else
		oflags |= O_TRUNC;

	for (exitval = 0; *argv; ++argv) {
		if ((fd = tee_open(*argv, oflags)) < 0) {
			warn("%s", *argv);
			exitval = 1;
		} else {
			add(fd, *argv);
		}
	}

	if (caph_enter() < 0)
		err(EXIT_FAILURE, "unable to enter capability mode");
	while ((rval = read(STDIN_FILENO, buf, BSIZE)) > 0)
		STAILQ_FOREACH(p, &head, entries) {
			n = rval;
			bp = buf;
			do {
				if ((wval = write(p->fd, bp, n)) == -1) {
					warn("%s", p->name);
					exitval = 1;
					break;
				}
				bp += wval;
			} while (n -= wval);
		}
	if (rval < 0)
		err(1, "read");
	exit(exitval);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: tee [-ai] [file ...]\n");
	exit(1);
}

static void
add(int fd, const char *name)
{
	struct entry *p;
	cap_rights_t rights;

	if (fd == STDOUT_FILENO) {
		if (caph_limit_stdout() == -1)
			err(EXIT_FAILURE, "unable to limit stdout");
	} else {
		cap_rights_init(&rights, CAP_WRITE, CAP_FSTAT);
		if (caph_rights_limit(fd, &rights) < 0)
			err(EXIT_FAILURE, "unable to limit rights");
	}

	if ((p = malloc(sizeof(struct entry))) == NULL)
		err(1, "malloc");
	p->fd = fd;
	p->name = name;
	STAILQ_INSERT_HEAD(&head, p, entries);
}

static int
tee_open(const char *path, int oflags)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };
	size_t pathlen;
	int fd;

	if ((fd = open(path, oflags, DEFFILEMODE)) >= 0)
		return (fd);

	if (errno != EOPNOTSUPP)
		return (-1);

	pathlen = strnlen(path, sizeof(sun.sun_path));
	if (pathlen >= sizeof(sun.sun_path))
		goto failed;

	/*
	 * For EOPNOTSUPP, we'll try again as a unix(4) socket.  Any errors here
	 * we'll just surface as the original EOPNOTSUPP since they may not have
	 * intended for this.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		goto failed;

	(void)strlcpy(&sun.sun_path[0], path, sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);

	if (connect(fd, (const struct sockaddr *)&sun, sun.sun_len) == 0)
		return (fd);

failed:
	if (fd >= 0)
		close(fd);
	errno = EOPNOTSUPP;
	return (-1);
}
