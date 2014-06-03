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

#include	<stdbool.h>
#include	<stdio.h>
#include	<string.h>

static int testnum = 1;

static void
runtest(const char *fname1, const char *mode1, const char *fname2,
    const char *mode2, bool success)
{
	FILE *fp1, *fp2;
	const char *fname2_print;

	fname2_print = fname2 != NULL ? fname2 : "<NULL>";
	fp1 = fopen(fname1, mode1);
	if (fp1 == NULL) {
		printf("not ok %d - fopen(\"%s\", \"%s\") failed\n",
		    testnum++, fname1, mode1);
		return;
	}
	fp2 = freopen(fname2, mode2, fp1);
	if (fp2 == NULL) {
		fclose(fp1);
		if (success)
			printf("not ok %d - "
			    "freopen(\"%s\", \"%s\", fopen(\"%s\", \"%s\")) "
			    "failed\n",
			    testnum++, fname2_print, mode2, fname1, mode1);
		else
			printf("ok %d - "
			    "freopen(\"%s\", \"%s\", fopen(\"%s\", \"%s\")) "
			    "failed\n",
			    testnum++, fname2_print, mode2, fname1, mode1);
		return;
	}
	if (success)
		printf("ok %d - "
		    "freopen(\"%s\", \"%s\", fopen(\"%s\", \"%s\")) "
		    "succeeded\n",
		    testnum++, fname2_print, mode2, fname1, mode1);
	else
		printf("not ok %d - "
		    "freopen(\"%s\", \"%s\", fopen(\"%s\", \"%s\")) "
		    "succeeded\n",
		    testnum++, fname2_print, mode2, fname1, mode1);
	fclose(fp2);
}

/*
 * Test program for freopen().
 */
int
main(int argc, char *argv[])
{
	printf("1..19\n");
	runtest("/dev/null", "r", NULL, "r", true);
	runtest("/dev/null", "w", NULL, "r", false);
	runtest("/dev/null", "r+", NULL, "r", true);
	runtest("/dev/null", "r", NULL, "w", false);
	runtest("/dev/null", "w", NULL, "w", true);
	runtest("/dev/null", "r+", NULL, "w", true);
	runtest("/dev/null", "r", NULL, "a", false);
	runtest("/dev/null", "w", NULL, "a", true);
	runtest("/dev/null", "r+", NULL, "a", true);
	runtest("/dev/null", "r", NULL, "r+", false);
	runtest("/dev/null", "w", NULL, "r+", false);
	runtest("/dev/null", "r+", NULL, "r+", true);
	runtest("/dev/null", "r", NULL, "w+", false);
	runtest("/dev/null", "w", NULL, "w+", false);
	runtest("/dev/null", "r+", NULL, "w+", true);
	runtest("/bin/sh", "r", NULL, "r", true);
	runtest("/bin/sh", "r", "/bin/sh", "r", true);
	runtest("/bin/sh", "r", "/dev/null", "r", true);
	runtest("/bin/sh", "r", "/dev/null", "w", true);

	return 0;
}

/* vim:ts=8:cin:sw=8
 *  */
