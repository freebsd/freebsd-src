/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Klara, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static int
exec_shortargs(char *argv[])
{
	char *flag_arg = argv[2];
	char *sentinel = argv[3];
	char * nargv[] = { argv[0], __DECONST(char *, "--spin"), flag_arg,
	    sentinel, NULL };
	char * const nenvp[] = { NULL };

	execve(argv[0], nargv, nenvp);
	err(1, "execve");
}

static int
exec_largeargs(char *argv[])
{
	char *flag_arg = argv[2];
	char *sentinel = argv[3];
	/*
	 * Account for each argument and their NUL terminator, as well as an
	 * extra NUL terminator.
	 */
	size_t bufsz = ARG_MAX -
	    ((strlen(argv[0]) + 1) + sizeof("--spin") + (strlen(flag_arg) + 1) +
	    (strlen(sentinel) + 1) + 1);
	char *s = NULL;
	char * nargv[] = { argv[0], __DECONST(char *, "--spin"), flag_arg, NULL,
	    sentinel, NULL };
	char * const nenvp[] = { NULL };

	/*
	 * Our heuristic may or may not be accurate, we'll keep trying with
	 * smaller argument sizes as needed until we stop getting E2BIG.
	 */
	do {
		if (s == NULL)
			s = malloc(bufsz + 1);
		else
			s = realloc(s, bufsz + 1);
		if (s == NULL)
			abort();
		memset(s, 'x', bufsz);
		s[bufsz] = '\0';
		nargv[3] = s;

		execve(argv[0], nargv, nenvp);
		bufsz--;
	} while (errno == E2BIG);
	err(1, "execve");
}

int
main(int argc, char *argv[])
{

	if (argc > 1 && strcmp(argv[1], "--spin") == 0) {
		int fd;

		if (argc < 4) {
			fprintf(stderr, "usage: %s --spin flagfile ...\n", argv[0]);
			return (1);
		}

		fd = open(argv[2], O_RDWR | O_CREAT, 0755);
		if (fd < 0)
			err(1, "%s", argv[2]);
		close(fd);

		for (;;) {
			sleep(1);
		}

		return (1);
	}

	if (argc != 4) {
		fprintf(stderr, "usage: %s [--short | --long] flagfile sentinel\n",
		    argv[0]);
		return (1);
	}

	if (strcmp(argv[1], "--short") == 0)
		exec_shortargs(argv);
	else
		exec_largeargs(argv);
}
