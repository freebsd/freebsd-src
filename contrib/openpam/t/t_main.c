/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id: t_main.c 651 2013-03-05 18:11:59Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "t.h"

const char *t_progname;

static int verbose;

void
t_verbose(const char *fmt, ...)
{
	va_list ap;

	if (verbose) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-v]\n", t_progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	const struct t_test **t_plan;
	const char *desc;
	int n, pass, fail;
	int opt;

#ifdef HAVE_SETLOGMASK
	/* suppress openpam_log() */
	setlogmask(LOG_UPTO(0));
#endif

	/* clean up temp files in case of premature exit */
	atexit(t_fcloseall);

	if ((t_progname = strrchr(argv[0], '/')) != NULL)
		t_progname++; /* one past the slash */
	else
		t_progname = argv[0];

	while ((opt = getopt(argc, argv, "v")) != -1)
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	/* prepare the test plan */
	if ((t_plan = t_prepare(argc, argv)) == NULL)
		errx(1, "no plan\n");

	/* count the tests */
	for (n = 0; t_plan[n] != NULL; ++n)
		/* nothing */;
	printf("1..%d\n", n);

	/* run the tests */
	for (n = pass = fail = 0; t_plan[n] != NULL; ++n) {
		desc = t_plan[n]->desc ? t_plan[n]->desc : "no description";
		if ((*t_plan[n]->func)(t_plan[n]->arg)) {
			printf("ok %d - %s\n", n + 1, desc);
			++pass;
		} else {
			printf("not ok %d - %s\n", n + 1, desc);
			++fail;
		}
	}

	/* clean up and exit */
	t_cleanup();
	exit(fail > 0 ? 1 : 0);
}
