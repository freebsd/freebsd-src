/*-
 * Copyright (c) 2013 Jilles Tjoelker
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
#include	<stdio.h>
#include	<string.h>

/*
 * O_ACCMODE is currently defined incorrectly. This is what it should be.
 * Various code depends on the incorrect value.
 */
#define CORRECT_O_ACCMODE (O_ACCMODE | O_EXEC)

static int testnum = 1;

static void
runtest(const char *fname, const char *mode)
{
	FILE *fp;
	int fd, flags, wantedflags;

	fp = fopen(fname, mode);
	if (fp == NULL) {
		printf("not ok %d - fopen(\"%s\", \"%s\") failed\n",
		    testnum++, fname, mode);
		printf("not ok %d - FD_CLOEXEC # SKIP\n",
		    testnum++);
		return;
	}
	fd = fileno(fp);
	if (fd < 0)
		printf("not ok %d - fileno() failed\n", testnum++);
	else
		printf("ok %d - fopen(\"%s\", \"%s\") and fileno() succeeded\n",
		    testnum++, fname, mode);
	if (fcntl(fd, F_GETFD) == (strchr(mode, 'e') != NULL ? FD_CLOEXEC : 0))
		printf("ok %d - FD_CLOEXEC flag correct\n", testnum++);
	else
		printf("not ok %d - FD_CLOEXEC flag incorrect\n", testnum++);
	flags = fcntl(fd, F_GETFL);
	if (strchr(mode, '+'))
		wantedflags = O_RDWR | (*mode == 'a' ? O_APPEND : 0);
	else if (*mode == 'r')
		wantedflags = O_RDONLY;
	else if (*mode == 'w')
		wantedflags = O_WRONLY;
	else if (*mode == 'a')
		wantedflags = O_WRONLY | O_APPEND;
	else
		wantedflags = -1;
	if (wantedflags == -1)
		printf("not ok %d - unrecognized mode\n", testnum++);
	else if ((flags & (CORRECT_O_ACCMODE | O_APPEND)) == wantedflags)
		printf("ok %d - correct access mode\n", testnum++);
	else
		printf("not ok %d - incorrect access mode\n", testnum++);
	fclose(fp);
}

/*
 * Test program for fopen().
 */
int
main(int argc, char *argv[])
{
	printf("1..45\n");
	runtest("/dev/null", "r");
	runtest("/dev/null", "r+");
	runtest("/dev/null", "w");
	runtest("/dev/null", "w+");
	runtest("/dev/null", "a");
	runtest("/dev/null", "a+");
	runtest("/dev/null", "re");
	runtest("/dev/null", "r+e");
	runtest("/dev/null", "we");
	runtest("/dev/null", "w+e");
	runtest("/dev/null", "ae");
	runtest("/dev/null", "a+e");
	runtest("/dev/null", "re+");
	runtest("/dev/null", "we+");
	runtest("/dev/null", "ae+");

	return 0;
}

/* vim:ts=8:cin:sw=8
 *  */
