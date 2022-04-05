/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 The FreeBSD Foundation
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

extern int	main_decode(int, char *[]);
extern int	main_encode(int, char *[]);

static int	search(const char *const);

enum coders {
	uuencode, uudecode, b64encode, b64decode
};

int
main(int argc, char *argv[])
{
	const char *const progname = getprogname();
	int coder = search(progname);

	if (coder == -1 && argc > 1) {
		argc--;
		argv++;
		coder = search(argv[0]);
	}
	switch (coder) {
	case uuencode:
	case b64encode:
		main_encode(argc, argv);
		break;
	case uudecode:
	case b64decode:
		main_decode(argc, argv);
		break;
	default:
		(void)fprintf(stderr,
		    "usage: %s <uuencode | uudecode> ...\n"
		    "       %s <b64encode | b64decode> ...\n",
		    progname, progname);
		exit(EX_USAGE);
	}
}

static int
search(const char *const progname)
{
#define DESIGNATE(item) [item] = #item
	const char *const known[] = {
		DESIGNATE(uuencode),
		DESIGNATE(uudecode),
		DESIGNATE(b64encode),
		DESIGNATE(b64decode)
	};

	for (size_t i = 0; i < nitems(known); i++)
		if (strcmp(progname, known[i]) == 0)
			return ((int)i);
	return (-1);
}
