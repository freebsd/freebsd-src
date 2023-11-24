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

/* Write and check read a file */

#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "stress.h"

static char path[128];
static int starting_dir;
static unsigned long size;

#define MAXSIZE 256 * 1024

int
setup(int nb)
{
	int64_t bl;
	int64_t in;
	int64_t reserve_in;
	int64_t reserve_bl;
	int pct;

	if (nb == 0) {
		getdf(&bl, &in);
		size = bl / op->incarnations / 1024;

		pct = 90;
		if (op->hog == 0)
			pct = random_int(1, 90);
		size = size / 100 * pct + 1;

		if (size > MAXSIZE)
			size = MAXSIZE;	/* arbitrary limit size pr. incarnation */

		/* Resource requirements: */
		while (size > 0) {
			reserve_in =  2 * op->incarnations + 1;
			reserve_bl = size * 1024 * op->incarnations +
				(512 * 1024 * op->incarnations) +
				  64 * 1024;
			if (reserve_bl <= bl && reserve_in <= in)
				break;
			size = size / 10 * 8;
		}
		if (size == 0)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("rw(size=%lu, incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
				size, op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(size);
		size = size * 1024;
	} else {
		size = getval();
		size = size * 1024;
	}
	if (size == 0)
		exit(0);

	umask(0);
	sprintf(path,"%s.%05d", getprogname(), getpid());
	(void)mkdir(path, 0770);
	if (chdir(path) == -1)
		err(1, "chdir(%s), %s:%d", path, __FILE__, __LINE__);
	if ((starting_dir = open(".", 0)) < 0)
		err(1, ".");

	return (0);
}

void
cleanup(void)
{
	if (starting_dir == 0)
		return;
	if (fchdir(starting_dir) == -1)
		err(1, "fchdir()");
	if (close(starting_dir) < 0)
		err(1, "close(starting_dir:%d)", starting_dir);

	(void)system("find . -delete");

	if (chdir("..") == -1)
		err(1, "chdir(..)");
	if (rmdir(path) == -1)
		err(1, "rmdir(%s), %s:%d", path, __FILE__, __LINE__);
	size = 0;
}

int
test(void)
{
	int buf[1024], index, to;
#ifdef TEST
	int i;
#endif
	int fd;
	char file[128];

	sprintf(file,"p%05d", getpid());
	if ((fd = creat(file, 0660)) == -1)
		err(1, "creat(%s)", file);

	to = sizeof(buf);
	index = 0;
	while (index < (int)size) {
		if (index + to > (int)size)
			to = size - index;
#ifdef TEST
		for (i = 0; i < to; i++)
			buf[i] = index + i;
#endif
		index += to;
		if (write(fd, buf, to) != to)
			err(1, "write(%s), %s:%d", file, __FILE__, __LINE__);
	}
	if (close(fd) == -1)
		err(1, "close(%s), %s:%d", file, __FILE__, __LINE__);

	if ((fd = open(file, O_RDONLY)) == -1)
		err(1, "open(%s), %s:%d", file, __FILE__, __LINE__);

	index = 0;
	while (index < (int)size && done_testing == 0) {
		if (index + to > (int)size)
			to = size - index;
		if (read(fd, buf, to) != to)
			err(1, "rw read. %s.%d", __FILE__, __LINE__);
#ifdef TEST
		for (i = 0; i < to; i++) {
			if (buf[i] != index + i) {
				fprintf(stderr,
					"%s, pid %d: expected %d @ %d, got %d\n",
					getprogname(), getpid(), index+i, index+i,
					buf[i]);
				exit(EXIT_FAILURE);
			}
		}
#endif
		index += to;
	}
	if (close(fd) == -1)
		err(1, "close(%s), %s:%d", file, __FILE__, __LINE__);
	if (unlink(file) == -1)
		err(1, "unlink(%s), %s:%d", file, __FILE__, __LINE__);
	return (0);
}
