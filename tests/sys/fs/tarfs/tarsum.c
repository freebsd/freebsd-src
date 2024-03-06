/*-
 * Copyright (c) 2024 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This program reads a tarball from stdin, recalculates the checksums of
 * all ustar records within it, and writes the result to stdout.
 */

#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool opt_v;

static int
verbose(const char *fmt, ...)
{
	va_list ap;
	int ret;

	if (!opt_v)
		return (0);
	va_start(ap, fmt);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return (ret);
}

static void
tarsum(FILE *in, const char *ifn, FILE *out, const char *ofn)
{
	union {
		uint8_t bytes[512];
		struct {
			uint8_t	prelude[148];
			char	checksum[8];
			uint8_t	interlude[101];
			char	magic[6];
			char	version[2];
			char	postlude[];
		};
	} ustar;
	unsigned long sum;
	off_t offset = 0;
	ssize_t ret;
	size_t i;

	for (;;) {
		if ((ret = fread(&ustar, 1, sizeof(ustar), in)) < 0)
			err(1, "%s", ifn);
		else if (ret == 0)
			break;
		else if ((size_t)ret < sizeof(ustar))
			errx(1, "%s: Short read", ifn);
		if (strcmp(ustar.magic, "ustar") == 0 &&
		    ustar.version[0] == '0' && ustar.version[1] == '0') {
			verbose("header found at offset %#lx\n", offset);
			verbose("current checksum %.*s\n",
			    (int)sizeof(ustar.checksum), ustar.checksum);
			memset(ustar.checksum, ' ', sizeof(ustar.checksum));
			for (sum = i = 0; i < sizeof(ustar); i++)
				sum += ustar.bytes[i];
			verbose("calculated checksum %#lo\n", sum);
			sprintf(ustar.checksum, "%#lo", sum);
		}
		if ((ret = fwrite(&ustar, 1, sizeof(ustar), out)) < 0)
			err(1, "%s", ofn);
		else if ((size_t)ret < sizeof(ustar))
			errx(1, "%s: Short write", ofn);
		offset += sizeof(ustar);
	}
	verbose("%lu bytes processed\n", offset);
}

static void
usage(void)
{
	fprintf(stderr, "usage: tarsum [-v] [-o output] [input]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *ifn, *ofn = NULL;
	FILE *in, *out;
	int opt;

	while ((opt = getopt(argc, argv, "o:v")) != -1) {
		switch (opt) {
		case 'o':
			ofn = optarg;
			break;
		case 'v':
			opt_v = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0 || strcmp(*argv, "-") == 0) {
		ifn = "stdin";
		in = stdin;
	} else if (argc == 1) {
		ifn = *argv;
		if ((in = fopen(ifn, "rb")) == NULL)
			err(1, "%s", ifn);
	} else {
		usage();
	}
	if (ofn == NULL || strcmp(ofn, "-") == 0) {
		ofn = "stdout";
		out = stdout;
	} else {
		if ((out = fopen(ofn, "wb")) == NULL)
			err(1, "%s", ofn);
	}
	tarsum(in, ifn, out, ofn);
	return (0);
}
