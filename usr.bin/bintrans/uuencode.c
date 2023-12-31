/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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

/*
 * uuencode [input] output
 *
 * Encode a file so it can be mailed to a remote system.
 */
#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bintrans.h"

static void encode(void);
static void do_base64_encode(encoding_t);
static int arg_to_col(const char *);
static void usage(void) __dead2;

static FILE *output;
static int mode;
static bool raw;
static char **av;
static int columns = 76;

int
main_base64_encode(encoding_t encoding, const char *in, const char *w)
{
	raw = 1;
	if (in != NULL && freopen(in, "r", stdin) == NULL)
		err(1, "%s", in);
	output = stdout;
	if (w != NULL)
		columns = arg_to_col(w);
	do_base64_encode(encoding);
	if (ferror(output))
		errx(1, "write error");
	exit(0);
}

int
main_encode(encoding_t encoding, int argc, char *argv[])
{
	struct stat sb;
	int ch;
	const char *outfile;

	outfile = NULL;

	while ((ch = getopt(argc, argv, "mo:rw:")) != -1) {
		switch (ch) {
		case 'm':
			encoding = BASE64;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'r':
			raw = true;
			break;
		case 'w':
			columns = arg_to_col(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	switch (argc) {
	case 2:			/* optional first argument is input file */
		if (!freopen(*argv, "r", stdin) || fstat(fileno(stdin), &sb))
			err(1, "%s", *argv);
#define	RWX	(S_IRWXU|S_IRWXG|S_IRWXO)
		mode = sb.st_mode & RWX;
		++argv;
		break;
	case 1:
#define	RW	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
		mode = RW & ~umask(RW);
		break;
	case 0:
	default:
		usage();
	}

	av = argv;

	if (outfile != NULL) {
		output = fopen(outfile, "w+");
		if (output == NULL)
			err(1, "unable to open %s for output", outfile);
	} else
		output = stdout;

	switch (encoding) {
		case UUE:
			encode();
			break;
		case BASE64:
		case BASE64URL:
			do_base64_encode(encoding);
			break;
		default:
			assert(!"main_encode: invalid encoding_t");
			/*NOTREACHED*/
	}

	if (ferror(output))
		errx(1, "write error");

	exit(0);
}

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')

/*
 * Copy from in to out, encoding in base64 as you go along.
 */
static void
do_base64_encode(encoding_t encoding)
{
	/*
	 * This buffer's length should be a multiple of 24 bits to avoid "="
	 * padding. Once it reached ~1 KB, further expansion didn't improve
	 * performance for me.
	 */
	unsigned char buf[1023];
	char buf2[sizeof(buf) * 2 + 1];
	size_t n;
	unsigned carry = 0;
	int rv, written;

	if (!raw)
		fprintf(output, "begin-base64 %o %s\n", mode, *av);
	while ((n = fread(buf, 1, sizeof(buf), stdin))) {
		rv = b64_encode(encoding, buf, n, buf2, nitems(buf2));
		if (rv == -1)
			errx(1, "do_base64_encode: error encoding base64");
		if (columns == 0) {
			fputs(buf2, output);
			continue;
		}
		for (int i = 0; i < rv; i += written) {
			written = fprintf(output, "%.*s", columns - carry,
			    &buf2[i]);

			carry = (carry + written) % columns;
			if (carry == 0)
				fputc('\n', output);
		}
	}
	if (columns == 0 || carry != 0)
		fputc('\n', output);
	if (!raw)
		fprintf(output, "====\n");
}

/*
 * Copy from in to out, encoding as you go along.
 */
static void
encode(void)
{
	int ch, n;
	char *p;
	char buf[80];

	if (!raw)
		(void)fprintf(output, "begin %o %s\n", mode, *av);
	while ((n = fread(buf, 1, 45, stdin))) {
		ch = ENC(n);
		if (fputc(ch, output) == EOF)
			break;
		for (p = buf; n > 0; n -= 3, p += 3) {
			/* Pad with nulls if not a multiple of 3. */
			if (n < 3) {
				p[2] = '\0';
				if (n < 2)
					p[1] = '\0';
			}
			ch = *p >> 2;
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = ((*p << 4) & 060) | ((p[1] >> 4) & 017);
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = ((p[1] << 2) & 074) | ((p[2] >> 6) & 03);
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = p[2] & 077;
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
		}
		if (fputc('\n', output) == EOF)
			break;
	}
	if (ferror(stdin))
		errx(1, "read error");
	if (!raw)
		(void)fprintf(output, "%c\nend\n", ENC('\0'));
}

static int
arg_to_col(const char *w)
{
	char *ep;
	long option;

	errno = 0;
	option = strtol(w, &ep, 10);
	if (option > INT_MAX)
		errno = ERANGE;
	else if (ep[0] != '\0')
		errno = EINVAL;
	if (errno != 0)
		err(2, NULL);

	if (option < 0) {
		errno = EINVAL;
		err(2, "columns argument must be non-negative");
	}
	return (option);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: uuencode [-m] [-o outfile] [infile] remotefile\n"
"       b64encode [-o outfile] [infile] remotefile\n");
	exit(1);
}
