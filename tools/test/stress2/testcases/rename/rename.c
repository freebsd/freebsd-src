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

static unsigned long size;

int
setup(int nb)
{
	int64_t in;
	int64_t bl;
	int64_t reserve_in;
	int64_t reserve_bl;

	umask(0);

	if (nb == 0) {
		getdf(&bl, &in);
		size = in / op->incarnations;

		if (size > 1000)
			size = 1000;	/* arbitrary limit number of files pr. dir */

		/* Resource requirements: */
		while (size > 0) {
			reserve_in =  2 * size * op->incarnations + 2 * op->incarnations;
			reserve_bl = 30 * size * op->incarnations;
			if (reserve_bl <= bl && reserve_in <= in)
				break;
			size = size / 2;
		}
		if (size == 0)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("rename(size=%lu, incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
				size, op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(size);
	} else {
		size = getval();
	}
	if (size == 0)
		exit(0);

	return (0);
}

void
cleanup(void)
{
}

static void
test_rename(void)
{
	int i, j;
	pid_t pid;
	char file1[128];
	char file2[128];
	int tfd;

	pid = getpid();
	for (i = 0; i < (int)size; i++) {
		sprintf(file1,"p%05d.%05d", pid, i);
		if ((tfd = open(file1, O_RDONLY|O_CREAT, 0660)) == -1)
			err(1, "openat(%s), %s:%d", file1, __FILE__, __LINE__);
		close(tfd);
	}
	for (j = 0; j < 100 && done_testing == 0; j++) {
		for (i = 0; i < (int)size; i++) {
			sprintf(file1,"p%05d.%05d", pid, i);
			sprintf(file2,"p%05d.%05d.togo", pid, i);
			if (rename(file1, file2) == -1)
				err(1, "rename(%s, %s). %s:%d", file1, file2,
						__FILE__, __LINE__);
		}
		for (i = 0; i < (int)size; i++) {
			sprintf(file1,"p%05d.%05d", pid, i);
			sprintf(file2,"p%05d.%05d.togo", pid, i);
			if (rename(file2, file1) == -1)
				err(1, "rename(%s, %s). %s:%d", file2, file1,
						__FILE__, __LINE__);
		}
	}

	for (i = 0; i < (int)size; i++) {
		sprintf(file1,"p%05d.%05d", pid, i);
		if (unlink(file1) == -1)
			err(1, "unlink(%s), %s:%d", file1, __FILE__, __LINE__);
	}
}

int
test(void)
{
	test_rename();

	return (0);
}
