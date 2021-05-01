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

/* Test lockf(3) */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "stress.h"

static pid_t pid;
static int fd;
static int freespace;
static char file[128];

static int
get(void) {
	int r, sem;

	do {
		r = lockf(fd, F_LOCK, 0);
	} while (r == -1 && errno == EINTR && done_testing == 0);
	if (r == -1)
		err(1, "lockf(%s, F_LOCK)", file);
	if (lseek(fd, 0, SEEK_SET) == -1) // XXX
		err(1, "lseek"); // XXX
	r = read(fd, &sem, sizeof(sem));
	if (r == -1)
		err(1, "get: read(%d)", fd);
	if (r == 0)
		errx(1, "get() read 0 bytes");
	if (r != sizeof(sem))
		errx(1, "get() size error: %d", r);
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	if (lockf(fd, F_ULOCK, 0) == -1)
		err(1, "lockf(%s, F_ULOCK)", file);
	return (sem);
}

static void
incr(void) {
	int r, sem;

	do {
		r = lockf(fd, F_LOCK, 0);
	} while (r == -1 && errno == EINTR && done_testing == 0);
	if (r == -1)
		err(1, "lockf(%s, F_LOCK)", file);
	if (read(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "incr: read(%d)", fd);
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	sem++;
	if (write(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "incr: read");
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	if (lockf(fd, F_ULOCK, 0) == -1)
		err(1, "lockf(%s, F_ULOCK)", file);
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
		reserve_in =    1 * op->incarnations;
		reserve_bl = 4096 * op->incarnations;
		freespace = (reserve_bl <= bl && reserve_in <= in);
		if (!freespace)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("lockf(incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
			    op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(freespace);
	} else {
		freespace = getval();
	}
	if (!freespace)
		exit(0);

        return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	int i;
	int sem = 0;

	sprintf(file, "lockf.0.%d", getpid());
	if ((fd = open(file,O_CREAT | O_TRUNC | O_RDWR, 0600)) == -1) {
		if (errno == ENOENT)
			return (0);
		else
			err(1, "creat(%s) %s:%d", file, __FILE__, __LINE__);
	}
	if (write(fd, &sem, sizeof(sem)) != sizeof(sem))
		err(1, "write");
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(2);
	}

	if (pid == 0) {	/* child */
		alarm(60);
		for (i = 0; i < 100 && done_testing == 0; i++) {
			while ((get() & 1) == 0 && done_testing == 0)
				;
			if (op->verbose > 3)
				printf("Child  %d, sem = %d\n", i, get()),
					fflush(stdout);
			incr();
		}
		_exit(0);
	} else {	/* parent */
		for (i = 0; i < 100 && done_testing == 0; i++) {
			while ((get() & 1) == 1 && done_testing == 0)
				;
			if (op->verbose > 3)
				printf("Parent %d, sem = %d\n", i, get()),
					fflush(stdout);
			incr();
		}
	}
	close(fd);
	if (done_testing == 1)
		kill(pid, SIGHUP);
	waitpid(pid, &i, 0);
	unlink(file);

        return (0);
}
