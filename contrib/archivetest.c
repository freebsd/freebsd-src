/*-
 * Copyright (c) 2019 Martin Matuska
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Archivetest verifies reading archives with libarchive
 *
 * It may be used to reproduce failures in testcases discovered by OSS-Fuzz
 * https://github.com/google/oss-fuzz/blob/master/projects/libarchive
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <archive.h>
#include <archive_entry.h>

#if defined __MINGW32__
#include <getopt.h>
#endif

static const char *errnostr(int e)
{
	char *estr;
	switch(e) {
		case ARCHIVE_EOF:
			estr = "ARCHIVE_EOF";
		break;
		case ARCHIVE_OK:
			estr = "ARCHIVE_OK";
		break;
		case ARCHIVE_WARN:
			estr = "ARCHIVE_WARN";
		break;
		case ARCHIVE_RETRY:
			estr = "ARCHIVE_RETRY";
		break;
		case ARCHIVE_FAILED:
			estr = "ARCHIVE_FAILED";
		break;
		case ARCHIVE_FATAL:
			estr = "ARCHIVE_FATAL";
		break;
		default:
			estr = "Unknown";
		break;
	}
	return (estr);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-f filename] [-h] [-q] [-s]\n", prog);
}

static void printhelp()
{
	fprintf(stdout, "archivetest: verify reading archives with "
	    "libarchive\n\n"
	    "Options:\n"
	    "  -f filename	Filename to verify\n"
	    "  -h		Show this help\n"
	    "  -q		Quiet mode\n"
	    "  -s		Verify only headers (skip data)\n\n"
	    "If no filename is specified, data is read from standard input.\n"
	    "\n%s\n", archive_version_details());
}

static int v_print(int verbose, const char *format, ...)
{
	int r = 0;

	if (verbose) {
		va_list args;

		va_start(args, format);
		r = vfprintf(stdout, format, args);
		va_end(args);
	}
	return (r);
}

int main(int argc, char *argv[])
{
	struct archive *a;
	struct archive_entry *entry;
	char *filename;
	const char *p;
	char buffer[4096];
	int c;
	int v, skip_data;
	int r = ARCHIVE_OK;
	int format_printed;

	filename = NULL;
	skip_data = 0;
	v = 1;

	while ((c = getopt (argc, argv, "f:hqs")) != -1) {
		switch (c) {
			case 'f':
				filename = optarg;
				break;
			case 'h':
				printhelp();
				exit(0);
			case 'q':
				v = 0;
				break;
			case 's':
				skip_data = 1;
				break;
			case '?':
				if (optopt == 'f')
					fprintf(stderr, "Option -%c requires "
					    "an argument.\n", optopt);
				else if (isprint(optopt))
					fprintf(stderr, "Unknown option '-%c'"
					    ".\n", optopt);
				else
					fprintf(stderr, "Unknown option "
					    "character '\\x%x'.\n", optopt);
				usage(argv[0]);
				exit(1);
				break;
			default:
				exit(1);
		}
	}

	a = archive_read_new();

	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	v_print(v, "Data source: ");

	if (filename == NULL) {
		v_print(v, "standard input\n");
		r = archive_read_open_fd(a, STDIN_FILENO, 4096);
	} else {
		v_print(v, "filename: %s\n", filename);
		r = archive_read_open_filename(a, filename, 4096);
	}

	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		fprintf(stderr, "Invalid or unsupported data source\n");
		exit(1);
	}

	format_printed = 0;
	c = 1;

	while (1) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_FATAL) {
			v_print(v, "Entry %d: fatal error reading "
			    "header\n", c);
			break;
		}
		if (!format_printed) {
			v_print(v, "Filter: %s\nFormat: %s\n",
			    archive_filter_name(a, 0), archive_format_name(a));
			format_printed = 1;
		}
		if (r == ARCHIVE_RETRY)
			continue;
		if (r == ARCHIVE_EOF)
			break;
		p = archive_entry_pathname(entry);
		v_print(v, "Entry %d: %s, pathname", c, errnostr(r));
		if (p == NULL || p[0] == '\0')
			v_print(v, " unreadable");
		else
			v_print(v, ": %s", p);
		v_print(v, ", data: ");
		if (skip_data) {
			v_print(v, "skipping");
		} else {
			while ((r = archive_read_data(a, buffer, 4096) > 0))
			;
			if (r == ARCHIVE_FATAL) {
				v_print(v, "ERROR\nError string: %s\n",
				    archive_error_string(a));
				break;
			}
			v_print(v, "OK");
		}
		v_print(v, "\n");
		c++;
	}

	v_print(v, "Last return code: %s (%d)\n", errnostr(r), r);
	if (r == ARCHIVE_EOF || r == ARCHIVE_OK) {
		archive_read_free(a);
		exit(0);
	}
	v_print(v, "Error string: %s\n", archive_error_string(a));
	archive_read_free(a);
	exit(2);
}
