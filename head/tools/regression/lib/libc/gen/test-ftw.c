/*-
 * Copyright (c) 2012 Jilles Tjoelker
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
 */

/*
 * Limited test program for nftw() as specified by IEEE Std. 1003.1-2008.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <unistd.h>

extern char **environ;

static char dir[] = "/tmp/testftw.XXXXXXXXXX";
static int failures;
static int ftwflags;

static void
cleanup(int ustatus __unused)
{
	int error, status;
	pid_t pid, waitres;
	const char *myargs[5];

	err_set_exit(NULL);
	myargs[0] = "rm";
	myargs[1] = "-rf";
	myargs[2] = "--";
	myargs[3] = dir;
	myargs[4] = NULL;
	error = posix_spawnp(&pid, myargs[0], NULL, NULL,
	    __DECONST(char **, myargs), environ);
	if (error != 0)
		warn("posix_spawnp rm");
	else {
		waitres = waitpid(pid, &status, 0);
		if (waitres != pid)
			warnx("waitpid rm failed");
		else if (status != 0)
			warnx("rm failed");
	}
}

static int
cb(const char *path, const struct stat *st, int type, struct FTW *f)
{

	switch (type) {
	case FTW_D:
		if ((ftwflags & FTW_DEPTH) == 0)
			return (0);
		break;
	case FTW_DP:
		if ((ftwflags & FTW_DEPTH) != 0)
			return (0);
		break;
	case FTW_SL:
		if ((ftwflags & FTW_PHYS) != 0)
			return (0);
		break;
	}
	warnx("unexpected path=%s type=%d f.level=%d\n",
	    path, type, f->level);
	failures++;
	return (0);
}

int
main(int argc, char *argv[])
{
	int fd;

	if (!mkdtemp(dir))
		err(2, "mkdtemp");

	err_set_exit(cleanup);

	fd = open(dir, O_DIRECTORY | O_RDONLY);
	if (fd == -1)
		err(2, "open %s", dir);

	if (mkdirat(fd, "d1", 0777) == -1)
		err(2, "mkdirat d1");

	if (symlinkat(dir, fd, "d1/looper") == -1)
		err(2, "symlinkat looper");

	ftwflags = FTW_PHYS;
	if (nftw(dir, cb, 10, ftwflags) == -1)
		err(2, "nftw FTW_PHYS");
	ftwflags = FTW_PHYS | FTW_DEPTH;
	if (nftw(dir, cb, 10, ftwflags) == -1)
		err(2, "nftw FTW_PHYS | FTW_DEPTH");
	ftwflags = 0;
	if (nftw(dir, cb, 10, ftwflags) == -1)
		err(2, "nftw 0");
	ftwflags = FTW_DEPTH;
	if (nftw(dir, cb, 10, ftwflags) == -1)
		err(2, "nftw FTW_DEPTH");

	close(fd);

	printf("PASS nftw()\n");

	cleanup(failures != 0);

	return (failures != 0);
}
