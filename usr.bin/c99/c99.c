/*-
 * Copyright (c) 2002 Tim J. Robbins.
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
 * c99 -- compile standard C programs
 *
 * This is essentially a wrapper around the system C compiler that forces
 * the compiler into C99 mode.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char **args;
u_int cargs, nargs;

void addarg(const char *item);
void usage(void);

int
main(int argc, char *argv[])
{
	int i;

	args = NULL;
	cargs = nargs = 0;

	addarg("cc");
	addarg("-std=iso9899:1999");
	addarg("-pedantic");
	for (i = 1; i < argc; i++)
		addarg(argv[i]);
	execv("/usr/bin/cc", args);
	err(1, "/usr/bin/cc");
}

void
addarg(const char *item)
{
	if (nargs + 1 > cargs) {
		cargs += 16;
		if ((args = realloc(args, sizeof(*args) * cargs)) == NULL)
			err(1, "malloc");
	}
	if ((args[nargs++] = strdup(item)) == NULL)
		err(1, "strdup");
	args[nargs] = NULL;
}

void
usage(void)
{
	fprintf(stderr,
"usage: c99 [-cEgs] [-D name[=value]] [-I directory] ... [-L directory] ...\n");
	fprintf(stderr,
"       [-o outfile] [-O optlevel] [-U name]... operand ...\n");
	exit(1);
}
