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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

static unsigned long size;
static char path[128];

int
setup(int nb)
{
	int64_t bl;
	int64_t in;
	int64_t reserve_bl;
	int64_t reserve_in;
	int pct;

	umask(0);
	path[0] = 0;
	if (nb == 0) {
		getdf(&bl, &in);
		size = in / op->incarnations;

		pct = 90;
		if (op->hog == 0)
			pct = random_int(1, 90);
		size = size / 100 * pct + 1;

		if (size > 32000 && op->hog == 0)
			size = 32000;	/* arbitrary limit number of files pr. dir */

		/* Resource requirements: */
		reserve_in =  2 * op->incarnations + 7;
		reserve_bl = 26 * size * op->incarnations;
		if (reserve_in > in || reserve_bl > bl)
			size = reserve_in = reserve_bl = 0;

		if (op->verbose > 1)
			printf("link(size=%lu, incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
				size, op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(size);
	} else {
		size = getval();
	}
	if (size == 0)
		exit (1);

	sprintf(path,"%s.%05d", getprogname(), getpid());
	if (mkdir(path, 0770) < 0)
		err(1, "mkdir(%s), %s:%d", path, __FILE__, __LINE__);

	if (chdir(path) == -1)
		err(1, "chdir(%s), %s:%d", path, __FILE__, __LINE__);

	return (0);
}

void
cleanup(void)
{
	if (size == 0)
		return;
	(void)chdir("..");
	if (path[0] != 0 && rmdir(path) == -1)
		warn("rmdir(%s), %s:%d", path, __FILE__, __LINE__);
}

int
test(void)
{
	pid_t pid;
	int fd, i, j;
	char file[128];
	char lfile[128];

	pid = getpid();
	for (j = 0; j < (int)size && done_testing == 0; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if (j == 0) {
			if ((fd = creat(file, 0660)) == -1) {
				if (errno != EINTR) {
					warn("creat(%s)", file);
					break;
				}
			}
			if (fd != -1 && close(fd) == -1)
				err(2, "close(%d)", j);
			strcpy(lfile, file);
		} else {
			if (link(lfile, file) == -1) {
				if (errno != EINTR) {
					warn("link(%s, %s)", lfile, file);
					break;
				}
			}
		}

	}

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}

	return (0);
}
