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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <err.h>

#include "stress.h"

static unsigned long size;

int
setup(int nb)
{
	int64_t in;
	int64_t bl;
	int64_t reserve_in;
	int64_t reserve_bl;
	int pct;

	if (nb == 0) {
		getdf(&bl, &in);
		size = in / op->incarnations;

		pct = 90;
		if (op->hog == 0)
			pct = random_int(1, 90);
		size = size / 100 * pct + 1;

		size = size % 200;	/* arbitrary limit depth */

		/* Resource requirements: */
		while (size > 0) {
			reserve_in =    1 * size * op->incarnations;
			reserve_bl = 4096 * size * op->incarnations;
			if (reserve_bl <= bl && reserve_in <= in)
				break;
			size = size / 2;
		}
		if (size == 0)
			reserve_bl = reserve_in = 0;

		if (op->verbose > 1)
			printf("mkdir(size=%lu, incarnations=%d). Free(%jdk, %jd), reserve(%jdk, %jd)\n",
				size, op->incarnations, bl/1024, in, reserve_bl/1024, reserve_in);
		reservedf(reserve_bl, reserve_in);
		putval(size);
	} else
		size = getval();

	if (size == 0)
		exit(0);

	return (0);
}

void
cleanup(void)
{
}

static void
mkDir(char *path, int level) {
	char newPath[MAXPATHLEN + 1];

	if (mkdir(path, 0770) == -1) {
		warn("mkdir(%s), level %d. %s:%d", path, level, __FILE__, __LINE__);
		size = level;
	} else
		chdir(path);

	if (done_testing == 1)
		size = level;

	if (level < (int)size) {
		sprintf(newPath,"d%d", level+1);
		mkDir(newPath, level+1);
	}
}

static void
rmDir(char *path, int level) {
	char newPath[MAXPATHLEN + 1];

	if (level < (int)size) {
		sprintf(newPath,"d%d", level+1);
		rmDir(newPath, level+1);
	}
	chdir ("..");
	if (rmdir(path) == -1) {
		err(1, "rmdir(%s), %s:%d", path, __FILE__, __LINE__);
	}
}

int
test(void)
{
	char path[MAXPATHLEN + 1];

	umask(0);
	sprintf(path,"p%05d.d%d", getpid(), 1);
	mkDir(path, 1);
	rmDir(path, 1);

	return (0);
}
