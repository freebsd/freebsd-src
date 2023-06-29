/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Jeremie Le Hen <jlh@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	LIBSTDBUF	"/usr/lib/libstdbuf.so"
#define	LIBSTDBUF32	"/usr/lib32/libstdbuf.so"

static int
appendenv(const char *key, const char *value)
{
	char *curval, *newpair;
	int ret;

	curval = getenv(key);
	if (curval == NULL)
		ret = asprintf(&newpair, "%s=%s", key, value);
	else
		ret = asprintf(&newpair, "%s=%s:%s", key, curval, value);
	if (ret > 0)
		ret = putenv(newpair);
	if (ret < 0)
		warn("Failed to set environment variable: %s", key);
	return (ret);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: stdbuf [-e 0|L|B|<sz>] [-i 0|L|B|<sz>] [-o 0|L|B|<sz>] "
	    "<cmd> [args ...]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *ibuf, *obuf, *ebuf;
	int i;

	ibuf = obuf = ebuf = NULL;
	while ((i = getopt(argc, argv, "e:i:o:")) != -1) {
		switch (i) {
		case 'e':
			ebuf = optarg;
			break;
		case 'i':
			ibuf = optarg;
			break;
		case 'o':
			obuf = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		exit(0);

	if (ibuf != NULL && setenv("_STDBUF_I", ibuf, 1) == -1)
		warn("Failed to set environment variable: %s=%s",
		    "_STDBUF_I", ibuf);
	if (obuf != NULL && setenv("_STDBUF_O", obuf, 1) == -1)
		warn("Failed to set environment variable: %s=%s",
		    "_STDBUF_O", obuf);
	if (ebuf != NULL && setenv("_STDBUF_E", ebuf, 1) == -1)
		warn("Failed to set environment variable: %s=%s",
		    "_STDBUF_E", ebuf);

	appendenv("LD_PRELOAD", LIBSTDBUF);
	appendenv("LD_32_PRELOAD", LIBSTDBUF32);

	execvp(argv[0], argv);
	err(2, "%s", argv[0]);
}
