/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 *
 */

/* Control program to run all the other test cases */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "stress.h"

#define MAXAV 10
static char *av[MAXAV];
static int loop = 1;

int
setup(int nb __unused)
{
	return (0);
}

void
cleanup(void)
{
}

static char **
mkargv(char *name)
{
	av[0] = name;
	av[1] = 0;
	return (av);
}

static void
clean(void)
{
	char buf[132];

	snprintf(buf, sizeof(buf),
		"cd %s; rm -rf syscall.[0-9]* fifo.[0-9]* creat.[0-9]* "
		"p[0-9]*.d1 df lock", op->cd);
	(void)system(buf);
}

int
test(void)
{
	struct tm *tm;
	pid_t *r;
	time_t	t;
	int i;
	int s;
	char fullpath[MAXPATHLEN+1];
	char ct[80];

	r = (pid_t *)malloc(op->argc * sizeof(pid_t));

	(void)time(&t);
	tm = localtime(&t);
	(void) strftime(ct, sizeof(ct), "%T", tm);
	printf("%s Loop #%d\n", ct, loop++);
	fflush(stdout);

	for (i = 0; i < op->argc; i++) {
		r[i] = 0;
		if (op->argv[i][0] == 0)
			continue;
		if ((r[i] = fork()) == 0) {
			snprintf(fullpath, sizeof(fullpath), "%s/%s", home,
				op->argv[i]);
			if (execv(fullpath, mkargv(basename(op->argv[i]))) == -1)
				err(1, "execl(%s), %s:%d", fullpath, __FILE__,
					__LINE__);
		}
		if (r[i] < 0)
			err(1, "fork(), %s:%d", __FILE__, __LINE__);
	}
	for (i = 0; i < op->argc; i++)
		if (r[i] != 0 && waitpid(r[i], &s, 0) == -1)
			err(1, "waitpid(%d), %s:%d", r[i], __FILE__, __LINE__);
	free(r);

	clean();

	return (0);
}
