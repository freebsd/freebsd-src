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
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

static char path1[128];
static char path2[] = "tmp";

static char rpath[128];
static char apath[128];

static int fd;
static int freespace;

int
setup(int nb)
{
	int64_t bl;
	int64_t in;
	int64_t reserve_bl;
	int64_t reserve_in;

	umask(0);

	if (nb == 0) {
		getdf(&bl, &in);

		/* Resource requirements: */
		reserve_in =    4 * op->incarnations;
		reserve_bl = 8192 * op->incarnations;
		freespace = (reserve_bl <= bl && reserve_in <= in);
		if (!freespace)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("openat(incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
			    op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(freespace);
	} else {
		freespace = getval();
	}
	if (!freespace)
		exit(0);

	sprintf(path1,"%s.%05d", getprogname(), getpid());
	if (mkdir(path1, 0770) < 0)
		err(1, "mkdir(%s), %s:%d", path1, __FILE__, __LINE__);
	if (chdir(path1) == -1)
		err(1, "chdir(%s), %s:%d", path1, __FILE__, __LINE__);

	if (mkdir(path2, 0770) < 0)
		err(1, "mkdir(%s), %s:%d", path2, __FILE__, __LINE__);
	if (chdir(path2) == -1)
		err(1, "chdir(%s), %s:%d", path2, __FILE__, __LINE__);
	if (getcwd(apath, sizeof(apath)) == NULL)
		err(1, "getcwd(%s), %s:%d", path2, __FILE__, __LINE__);

	if (chdir("..") == -1)
		err(1, "chdir(%s), %s:%d", path1, __FILE__, __LINE__);

	if ((fd = open(path2, O_RDONLY)) == -1)
		err(1, "open(%s), %s:%d", path2, __FILE__, __LINE__);

	strcpy(rpath, "tmp");
	return (0);
}

void
cleanup(void)
{
	if (rmdir(path2) == -1)
		warn("rmdir(%s), %s:%d", path2, __FILE__, __LINE__);
	(void)chdir("..");
	if (rmdir(path1) == -1)
		warn("rmdir(%s), %s:%d", path1, __FILE__, __LINE__);
}

static void
test_openat(void)
{
	pid_t pid;
	int i;
	int tfd;
	char file[128];
	char p[128];

	pid = getpid();
	for (i = 0; i < 100 && done_testing == 0; i++) {
		sprintf(file,"p%05d.%05d", pid, i);
		if ((tfd = openat(fd, file, O_RDONLY|O_CREAT, 0660)) == -1)
			err(1, "openat(%s), %s:%d", file, __FILE__, __LINE__);
		close(tfd);
		strcpy(p, "tmp/");
		strcat(p, file);
		if (unlink(p) == -1)
			err(1, "unlink(%s), %s:%d", p, __FILE__, __LINE__);
	}
}

static void
test_renameat(void)
{
	pid_t pid;
	int i;
	int tfd;
	char file[128];
	char file2[128];

	pid = getpid();
	for (i = 0; i < 100 && done_testing == 0; i++) {
		sprintf(file,"p%05d.%05d", pid, i);
		if ((tfd = openat(fd, file, O_RDONLY|O_CREAT, 0660)) == -1)
			err(1, "openat(%s), %s:%d", file, __FILE__, __LINE__);
		close(tfd);

		sprintf(file2,"p%05d.%05d.togo", pid, i);
		if (renameat(fd, file, fd, file2) == -1)
			err(1, "renameat(%s)", file2);

		sprintf(file2,"tmp/p%05d.%05d.togo", pid, i);
		if (unlink(file2) == -1)
			err(1, "unlink(%s), %s:%d", file2, __FILE__, __LINE__);
	}
}

static void
test_unlinkat(void)
{
	pid_t pid;
	int i;
	int tfd;
	char file[128];

	pid = getpid();
	for (i = 0; i < 100 && done_testing == 0; i++) {
		sprintf(file,"p%05d.%05d", pid, i);
		if ((tfd = openat(fd, file, O_RDONLY|O_CREAT, 0660)) == -1)
			err(1, "openat(%s), %s:%d", file, __FILE__, __LINE__);
		close(tfd);
		if (unlinkat(fd, file, 0) == -1)
			err(1, "unlinkat(%s), %s:%d", file, __FILE__, __LINE__);
	}
}

int
test(void)
{
	test_openat();
	test_renameat();
	test_unlinkat();

	return (0);
}
