/*-
 * Copyright (c) 2014 Jilles Tjoelker
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include	<fcntl.h>
#include	<stdbool.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>

static int testnum = 1;

static void
runtest(const char *fname, int intmode, const char *strmode, bool success)
{
	FILE *fp;
	int fd;

	fd = open(fname, intmode);
	if (fd == -1) {
		printf("not ok %d - open(\"%s\", %#x) failed\n",
		    testnum++, fname, intmode);
		return;
	}
	fp = fdopen(fd, strmode);
	if (fp == NULL) {
		close(fd);
		if (success)
			printf("not ok %d - "
			    "fdopen(open(\"%s\", %#x), \"%s\") failed\n",
			    testnum++, fname, intmode, strmode);
		else
			printf("ok %d - "
			    "fdopen(open(\"%s\", %#x), \"%s\") failed\n",
			    testnum++, fname, intmode, strmode);
		return;
	}
	if (success)
		printf("ok %d - "
		    "fdopen(open(\"%s\", %#x), \"%s\") succeeded\n",
		    testnum++, fname, intmode, strmode);
	else
		printf("not ok %d - "
		    "fdopen(open(\"%s\", %#x), \"%s\") succeeded\n",
		    testnum++, fname, intmode, strmode);
	fclose(fp);
}

/*
 * Test program for fdopen().
 */
int
main(int argc, char *argv[])
{
	printf("1..19\n");
	runtest("/dev/null", O_RDONLY, "r", true);
	runtest("/dev/null", O_WRONLY, "r", false);
	runtest("/dev/null", O_RDWR, "r", true);
	runtest("/dev/null", O_RDONLY, "w", false);
	runtest("/dev/null", O_WRONLY, "w", true);
	runtest("/dev/null", O_RDWR, "w", true);
	runtest("/dev/null", O_RDONLY, "a", false);
	runtest("/dev/null", O_WRONLY, "a", true);
	runtest("/dev/null", O_RDWR, "a", true);
	runtest("/dev/null", O_RDONLY, "r+", false);
	runtest("/dev/null", O_WRONLY, "r+", false);
	runtest("/dev/null", O_RDWR, "r+", true);
	runtest("/dev/null", O_RDONLY, "w+", false);
	runtest("/dev/null", O_WRONLY, "w+", false);
	runtest("/dev/null", O_RDWR, "w+", true);
	runtest("/bin/sh", O_EXEC, "r", false);
	runtest("/bin/sh", O_EXEC, "w", false);
	runtest("/bin/sh", O_EXEC, "r+", false);
	runtest("/bin/sh", O_EXEC, "w+", false);

	return 0;
}

/* vim:ts=8:cin:sw=8
 *  */
