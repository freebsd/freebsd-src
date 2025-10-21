/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011-2014, Mike Kazantsev
 * All rights reserved.
 */

#include "bsdcat_platform.h"

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <archive.h>
#include <archive_entry.h>

#include "bsdcat.h"
#include "lafe_err.h"

#define	BYTES_PER_BLOCK	(20*512)

static struct archive *a;
static struct archive_entry *ae;
static const char *bsdcat_current_path;
static int exit_status = 0;


static __LA_NORETURN void
usage(FILE *stream, int eval)
{
	const char *p;
	p = lafe_getprogname();
	fprintf(stream,
	    "Usage: %s [-h] [--help] [--version] [--] [filenames...]\n", p);
	exit(eval);
}

static __LA_NORETURN void
version(void)
{
	printf("bsdcat %s - %s \n",
	    BSDCAT_VERSION_STRING,
	    archive_version_details());
	exit(0);
}

static void
bsdcat_print_error(void)
{
	lafe_warnc(0, "%s: %s",
	    bsdcat_current_path, archive_error_string(a));
	exit_status = 1;
}

static void
bsdcat_next(void)
{
	if (a != NULL) {
		if (archive_read_close(a) != ARCHIVE_OK)
			bsdcat_print_error();
		archive_read_free(a);
	}

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_empty(a);
	archive_read_support_format_raw(a);
}

static void
bsdcat_read_to_stdout(const char* filename)
{
	int r;

	if (archive_read_open_filename(a, filename, BYTES_PER_BLOCK)
	    != ARCHIVE_OK)
		bsdcat_print_error();
	else if (r = archive_read_next_header(a, &ae),
		 r != ARCHIVE_OK && r != ARCHIVE_EOF)
		bsdcat_print_error();
	else if (r == ARCHIVE_EOF)
		/* for empty payloads don't try and read data */
		;
	else if (archive_read_data_into_fd(a, 1) != ARCHIVE_OK)
		bsdcat_print_error();
	if (archive_read_close(a) != ARCHIVE_OK)
		bsdcat_print_error();
	archive_read_free(a);
	a = NULL;
}

int
main(int argc, char **argv)
{
	struct bsdcat *bsdcat, bsdcat_storage;
	int c;

	bsdcat = &bsdcat_storage;
	memset(bsdcat, 0, sizeof(*bsdcat));

#if defined(HAVE_SIGACTION) && defined(SIGCHLD)
	{ /* Do not ignore SIGCHLD. */
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGCHLD, &sa, NULL);
	}
#endif

	lafe_setprogname(*argv, "bsdcat");

	bsdcat->argv = argv;
	bsdcat->argc = argc;

	while ((c = bsdcat_getopt(bsdcat)) != -1) {
		switch (c) {
		case 'h':
			usage(stdout, 0);
			/* NOTREACHED */
			/* Fallthrough */
		case OPTION_VERSION:
			version();
			/* NOTREACHED */
			/* Fallthrough */
		default:
			usage(stderr, 1);
			/* Fallthrough */
			/* NOTREACHED */
		}
	}

	bsdcat_next();
	if (*bsdcat->argv == NULL) {
		bsdcat_current_path = "<stdin>";
		bsdcat_read_to_stdout(NULL);
	} else {
		while (*bsdcat->argv) {
			bsdcat_current_path = *bsdcat->argv++;
			bsdcat_read_to_stdout(bsdcat_current_path);
			bsdcat_next();
		}
		archive_read_free(a); /* Help valgrind & friends */
	}

	exit(exit_status);
}
