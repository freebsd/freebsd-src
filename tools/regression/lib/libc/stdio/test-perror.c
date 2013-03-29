/*-
 * Copyright (c) 2002 Tim J. Robbins
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
 * Test program for perror() as specified by IEEE Std. 1003.1-2001 and
 * ISO/IEC 9899:1999.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void cleanup(void);
static char tmpfil[PATH_MAX];

int
main(int argc, char *argv[])
{
	char lbuf[512];
	int i;
	char *s;

	printf("1..1\n");

	strcpy(tmpfil, _PATH_TMP "perror.XXXXXXXX");
	if (mkstemp(tmpfil) < 0)
		err(1, "mkstemp");
	atexit(cleanup);
	/* Reopen stderr on a file descriptor other than 2. */
	fclose(stderr);
	for (i = 0; i < 3; i++)
		dup(0);
	if (freopen(tmpfil, "r+", stderr) == NULL)
		err(1, "%s", tmpfil);

	/*
	 * Test that perror() doesn't call strerror() (4.4BSD bug),
	 * the two ways of omitting a program name, and the formatting when
	 * a program name is specified.
	 */
	s = strerror(ENOENT);
	assert(strcmp(s, "No such file or directory") == 0);
	errno = EPERM;
	perror(NULL);
	perror("");
	perror("test-perror");
	assert(strcmp(s, "No such file or directory") == 0);

	/*
	 * Read it back to check...
	 */
	rewind(stderr);
	s = fgets(lbuf, sizeof(lbuf), stderr);
	assert(s != NULL);
	assert(strcmp(s, "Operation not permitted\n") == 0);
	s = fgets(lbuf, sizeof(lbuf), stderr);
	assert(s != NULL);
	assert(strcmp(s, "Operation not permitted\n") == 0);
	s = fgets(lbuf, sizeof(lbuf), stderr);
	assert(s != NULL);
	assert(strcmp(s, "test-perror: Operation not permitted\n") == 0);
	s = fgets(lbuf, sizeof(lbuf), stderr);
	assert(s == NULL);
	fclose(stderr);

	printf("ok 1 - perror()\n");

	return (0);
}

static void
cleanup(void)
{

	unlink(tmpfil);
}
