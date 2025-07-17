/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Hudson River Trading LLC
 * Written by: George V. Neville-Neil <gnn@FreeBSD.org>
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
#include <sys/errno.h>

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <capsicum_helpers.h>

static void usage(void) __dead2;

int
main(int argc, char **argv)
{
	char *cp;
	char *errstr;
	long errnum;

	(void) setlocale(LC_MESSAGES, "");

	caph_cache_catpages();
	if (caph_limit_stdio() < 0 || caph_enter() < 0)
		err(EXIT_FAILURE, "capsicum");

	if (argc != 2)
		usage();

	errno = 0;

	errnum = strtol(argv[1], &cp, 0);

	if (errno != 0)
		err(EXIT_FAILURE, NULL);

	if ((errstr = strerror(errnum)) == NULL)
		err(EXIT_FAILURE, NULL);

	printf("%s\n", errstr);

	exit(EXIT_SUCCESS);
}

static void
usage(void)
{
	fprintf(stderr, "usage: perror number\n");
	exit(EXIT_FAILURE);
}

