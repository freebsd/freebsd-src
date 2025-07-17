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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

static char path[MAXPATHLEN+1];
static int bufsize, freespace;

static void
reader(void) {
	fd_set set;
	struct timeval tv;
	int *buf, fd, n;

	setproctitle("reader");
	if ((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
		err(1, "open(%s)", path);
	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	n = 0;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (select(fd + 1, &set, NULL, NULL, &tv) == 1) {
		if ((n = read(fd, buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
	}
	close(fd);
	free(buf);
}

static void
writer(void) {
	int *buf, fd;

	setproctitle("writer");
	if ((fd = open(path, O_WRONLY)) < 0) {
		unlink(path);
		err(1, "open(%s)", path);
	}
	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	memset(buf, 0, bufsize);

	if (write(fd, buf, bufsize) < 0)
		err(1, "write(%d), %s:%d", fd, __FILE__, __LINE__);
	close(fd);
	free(buf);
}

int
setup(int nb)
{
	int64_t bl;
	int64_t in;
	int64_t reserve_bl;
	int64_t reserve_in;

	if (nb == 0) {
		getdf(&bl, &in);

		/* Resource requirements: */
		reserve_in =  200 * op->incarnations;
		reserve_bl = 2048 * op->incarnations;
		freespace = (reserve_bl <= bl && reserve_in <= in);
		if (!freespace)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("mkfifo(incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
			    op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(freespace);
		fflush(stdout);
	} else {
		freespace = getval();
	}
	if (!freespace)
		_exit(0);
	bufsize = 2 << random_int(2, 12);

	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	pid_t pid;
	int i, status;

	for (i = 0; i < 100; i++) {
		if (sprintf(path, "fifo.%d.%d", getpid(), i) < 0)
			err(1, "sprintf()");
		if (mkfifo(path, 0600) < 0)
			err(1, "mkfifo(%s)", path);
	}
	for (i = 0; i < 100; i++) {
		if (sprintf(path, "fifo.%d.%d", getpid(), i) < 0)
			err(1, "sprintf()");
		if (unlink(path) < 0)
			err(1, "unlink(%s)", path);
	}

	if (sprintf(path, "fifo.%d", getpid()) < 0)
		err(1, "sprintf()");
	if (mkfifo(path, 0600) < 0)
		err(1, "mkfifo(%s)", path);

	if ((pid = fork()) == 0) {
		writer();
		_exit(EXIT_SUCCESS);

	} else if (pid > 0) {
		reader();
		kill(pid, SIGINT);
		if (waitpid(pid, &status, 0) == -1)
			warn("waitpid(%d)", pid);
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	unlink(path);

	return (0);
}
