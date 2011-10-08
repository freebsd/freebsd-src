/*
 * Copyright (C) 2004, 2005, 2007, 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: genrandom.c,v 1.7 2010-05-17 23:51:04 tbox Exp $ */

/*! \file */
#include <config.h>

#include <isc/commandline.h>
#include <isc/print.h>
#include <isc/stdlib.h>
#include <isc/util.h>

#include <stdio.h>
#include <string.h>

const char *program = "genrandom";

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "usage: %s [-n 2..9] k file\n", program);
	exit(1);
}

static void
generate(char *filename, unsigned int bytes) {
	FILE *fp;

	fp = fopen(filename, "w");
	if (fp == NULL) {
		printf("failed to open %s\n", filename);
		exit(1);
	}

	while (bytes > 0) {
#ifndef HAVE_ARC4RANDOM
		unsigned short int x = (rand() & 0xFFFF);
#else
		unsigned short int x = (arc4random() & 0xFFFF);
#endif
		unsigned char c = x & 0xFF;
		if (putc(c, fp) == EOF) {
			printf("error writing to %s\n", filename);
			exit(1);
		}
		c = x >> 8;
		if (putc(c, fp) == EOF) {
			printf("error writing to %s\n", filename);
			exit(1);
		}
		bytes -= 2;
	}
	fclose(fp);
}

int
main(int argc, char **argv) {
	unsigned int bytes;
	unsigned int k;
	char *endp;
	int c, i, n = 1;
	size_t len;
	char *name;

	isc_commandline_errprint = ISC_FALSE;

	while ((c = isc_commandline_parse(argc, argv, "hn:")) != EOF) {
		switch (c) {
		case 'n':
			n = strtol(isc_commandline_argument, &endp, 10);
			if ((*endp != 0) || (n <= 1) || (n > 9))
				usage();
			break;

		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
		case 'h':
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (isc_commandline_index + 2 != argc)
		usage();

	k = strtoul(argv[isc_commandline_index++], &endp, 10);
	if (*endp != 0)
		usage();
	bytes = k << 10;

#ifndef HAVE_ARC4RANDOM
	srand(0x12345678);
#endif
	if (n == 1) {
		generate(argv[isc_commandline_index], bytes);
		return (0);
	}

	len = strlen(argv[isc_commandline_index]) + 2;
	name = (char *) malloc(len);
	if (name == NULL) {
		perror("malloc");
		exit(1);
	}

	for (i = 1; i <= n; i++) {
		snprintf(name, len, "%s%d", argv[isc_commandline_index], i);
		generate(name, bytes);
	}
	free(name);

	return (0);
}
