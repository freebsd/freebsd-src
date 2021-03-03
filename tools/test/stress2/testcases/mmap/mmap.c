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
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

static char path[128];

#define INPUTFILE "/bin/date"

int
setup(int nb)
{
	int64_t bl;
	int64_t in;
	int64_t reserve_bl;
	int64_t reserve_in;
	int freespace;

	if (nb == 0) {
		getdf(&bl, &in);

		/* Resource requirements: */
		reserve_in =    2 * op->incarnations;
		reserve_bl = 20480 * op->incarnations;
		freespace = (reserve_bl <= bl && reserve_in <= in);
		if (!freespace)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("mmap(incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
			    op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(freespace);
	} else {
		freespace = getval();
	}
	if (!freespace)
		exit(0);
	umask(0);

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
	(void)chdir("..");
	if (rmdir(path) == -1) {
		warn("rmdir(%s), %s:%d", path, __FILE__, __LINE__);
	}
}

int
test(void)
{
	struct stat statbuf;
	pid_t pid;
	char file[128];
	int fdin, fdout;
	int i;
	char *src, *dst;

	pid = getpid();
	for (i = 0; i < 100 && done_testing == 0; i++) {
		sprintf(file,"p%05d.%05d", pid, i);

		if ((fdin = open(INPUTFILE, O_RDONLY)) < 0)
			err(1, INPUTFILE);

		if ((fdout = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
			err(1, "%s", file);

		if (fstat(fdin, &statbuf) < 0)
			err(1, "fstat error");

		if (lseek(fdout, statbuf.st_size - 1, SEEK_SET) == -1)
			err(1, "lseek error");

		/* write a dummy byte at the last location */
		if (write(fdout, "", 1) != 1)
			err(1, "write error");

		if ((src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0)) ==
			(caddr_t) - 1)
			err(1, "mmap error for input");

		if ((dst = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fdout, 0)) == (caddr_t) - 1)
			err(1, "mmap error for output");

		memcpy(dst, src, statbuf.st_size);

		if (munmap(src, statbuf.st_size) == -1)
			err(1, "munmap");
		close(fdin);

		if (munmap(dst, statbuf.st_size) == -1)
			err(1, "munmap");
		close(fdout);

		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);
	}

	return (0);
}
