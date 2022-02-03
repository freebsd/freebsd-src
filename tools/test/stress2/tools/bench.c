/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Dell EMC Isilon
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 3
#define SYNC 0

static void cr1(void);
static void cr2(void);
static void cr3(void);
static void rn1(void);
static void rw1(void);
static void rw2(void);
static void (*functions[])(void) = {&cr1, &cr2, &cr3, &rn1, &rw1, &rw2};

static volatile u_int *share;
static int tests;

static void
cr1(void)
{
	int fd, i, j;
	int loops = 9000;
	char file[128];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);
	setproctitle("%s", __func__);
	for (j = 0; j < 10; j++) {
		for (i = 0; i < loops; i++) {
			snprintf(file, sizeof(file), "%s.%06d.%03d",
			    __func__, getpid(), i);
			if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(1, "open(%s)", file);
			close(fd);
			if (i % 1000 == 0)
				usleep(100);
		}
		for (i = 0; i < loops; i++) {
			snprintf(file, sizeof(file), "%s.%06d.%03d",
			    __func__, getpid(), i);
			if (unlink(file) == -1)
				err(1, "unlink(%s)", file);
		}
	}
}

static void
cr2(void)
{
	int fd, i, j;
	char file[1024];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);
	setproctitle("%s", __func__);
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 40000; i++) {
			snprintf(file, sizeof(file), "%s.%06d.%03d",
			    __func__, getpid(), i);
			if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(1, "open(%s)", file);
			close(fd);
			if (unlink(file) == -1)
				err(1, "unlink(%s)", file);
			if (i % 1000 == 0)
				usleep(100);
		}
	}
}

static void
cr3(void)
{
	int fd, i, j;
	int loops = 10000;
	char file[1024], path[1024];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);
	setproctitle("%s", __func__);
	getcwd(path, sizeof(path));
	for (j = 0; j < 7; j++) {
		for (i = 0; i < loops; i++) {
			snprintf(file, sizeof(file), "%s/%s.%06d.%03d",
			    path, __func__, getpid(), i);
			if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(1, "open(%s)", file);
			close(fd);
			if (i % 1000 == 0)
				usleep(100);
		}
		for (i = 0; i < loops; i++) {
			snprintf(file, sizeof(file), "%s/%s.%06d.%03d",
			    path, __func__, getpid(), i);
			if (unlink(file) == -1)
				err(1, "unlink(%s)", file);
		}
	}
}

static void
rn1(void)
{
	int fd, i, j;
	int loops = 10000;
	char file[128], new[128];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);
	setproctitle("%s", __func__);

	for (j = 0; j < 8; j++) {
		for (i = 0; i < loops; i++) {
			snprintf(file, sizeof(file), "%s.%06d.%03d",
			    __func__, getpid(), i);
			if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(1, "open(%s)", file);
			close(fd);
			snprintf(new, sizeof(new), "%s.%06d.%03d.new",
			    __func__, getpid(), i);
			if (rename(file, new) == -1)
				err(1, "rename(%s, %s)", file, new);
			if (unlink(new) == -1)
				err(1, "unlink(%s)", new);
			if (i % 1000 == 0)
				usleep(100);
		}
	}
}

static void
rw1(void)
{
	int fd, i;
	int loops = 10000;
	char buf[512], file[128];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);

	setproctitle("%s", __func__);
        memset(buf, 0, sizeof(buf));
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
                    DEFFILEMODE)) == -1)
                        err(1, "open(%s)", file);
                if (write(fd, buf, sizeof(buf)) != sizeof(buf))
                        err(1, "write(%s)", file);
                close(fd);
        }
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if ((fd = open(file, O_RDONLY)) == -1)
                        err(1, "open(%s)", file);
                if (read(fd, buf, sizeof(buf)) != sizeof(buf))
                        err(1, "write(%s)", file);
                close(fd);
                usleep(100);
        }
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if (unlink(file) == -1)
                        err(1, "unlink(%s)", file);
        }
}

static void
rw2(void)
{
	int fd, i;
	int loops = 8000;
	int siz = 4096;
	char *buf, file[128];

	setproctitle("%s sync", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)tests * PARALLEL)
		usleep(100);

	setproctitle("%s", __func__);
	buf = calloc(1, siz);
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
                    DEFFILEMODE)) == -1)
                        err(1, "open(%s)", file);
                if (write(fd, buf, siz) != siz)
                        err(1, "write(%s)", file);
                close(fd);
        }
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if ((fd = open(file, O_RDONLY)) == -1)
                        err(1, "open(%s)", file);
                if (read(fd, buf, siz) != siz)
                        err(1, "write(%s)", file);
                close(fd);
                usleep(100);
        }
        for (i = 0; i < loops; i++) {
                snprintf(file, sizeof(file), "rw1.%06d.%03d", getpid(), i);
                if (unlink(file) == -1)
                        err(1, "unlink(%s)", file);
        }
}

static void
spawn(void f(), int idx)
{
	pid_t pids[PARALLEL];
	int i, status;
	char dir[128];

	snprintf(dir, sizeof(dir), "f%d.%d.d",getpid(), idx);
	rmdir(dir);
	if (mkdir(dir, 0770) == -1)
		err(1, "mkdir(%s)", dir);
	if (chdir(dir) == -1)
		err(1, "chdir(%s)", dir);
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0) {
			f();
			_exit(0);
		}
		if (pids[i] == -1)
			err(1, "fork(). %s:%d", __func__, __LINE__);
	}
	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], &status, 0) != pids[i])
			err(1, "waitpid(). %s:%d", __func__, __LINE__);
	}
	if (chdir("..") == -1)
		err(1, "chdir(..)");
	if (rmdir(dir) == -1)
		err(1, "rmdir(%s)", dir);

}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-t]\n", getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	pid_t *pids;
	struct timeval t1, t2, diff;
	size_t len;
	time_t start __unused;
	int ch, i, status, timing;

	timing = 0;
	while ((ch = getopt(argc, argv, "t")) != -1)
		switch(ch) {
		case 't':
			timing = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	setproctitle("%s", __func__);
	tests = (int)(sizeof(functions) / sizeof(functions[0]));
	pids = malloc(tests * sizeof(pid_t));
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	gettimeofday(&t1, NULL);
	for (i = 0; i < tests; i++) {
		if ((pids[i] = fork()) == 0) {
			start = time(NULL);
			spawn(functions[i], i);
#if defined(DEBUG)
			fprintf(stderr, "%d: %ld elapsed\n", i ,
			    (long)(time(NULL) - start));
#endif
			_exit(0);
		}
		if (pids[i] == -1)
			err(1, "fork(). %s:%d", __func__, __LINE__);
	}
	for (i = 0; i < tests; i++) {
		if (waitpid(pids[i], &status, 0) != pids[i])
			err(1, "waitpid(%d). i=%d %s:%d", pids[i], i,
					__func__, __LINE__);
	}
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &diff);
	if (timing == 1)
		printf("%jd.%06ld\n",(intmax_t)diff.tv_sec, diff.tv_usec);

	return (0);
}
