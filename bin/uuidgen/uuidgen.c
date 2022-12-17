/*
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2022 Tobias C. Berner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/capsicum.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid.h>

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: uuidgen [-1] [-r] [-n count] [-o filename]\n");
	exit(1);
}

static int
uuidgen_v4(struct uuid *store, int count)
{
	int size;
	struct uuid *item;

	if (count < 1) {
		errno = EINVAL;
		return (-1);
	}
	size = sizeof(struct uuid) * count;
	arc4random_buf(store, size);
	item = store;
	for (int i = 0; i < count; ++i) {
		/*
		 * Set the two most significant bits (bits 6 and 7) of the
		 * clock_seq_hi_and_reserved to zero and one, respectively.
		 */
		item->clock_seq_hi_and_reserved &= ~(3 << 6);
		item->clock_seq_hi_and_reserved |= (2 << 6);
		/*
		 * Set the four most significant bits (bits 12 through 15) of
		 * the time_hi_and_version field to the 4-bit version number
		 * from  Section 4.1.3.
		 */
		item->time_hi_and_version &= ~(15 << 12);
		item->time_hi_and_version |= (4 << 12);
		item++;
	};
	return (0);
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	uuid_t *store, *uuid;
	char *p;
	int ch, count, i, iterate, status, version;

	count = -1;  /* no count yet */
	fp = stdout; /* default output file */
	iterate = 0; /* not one at a time */
	version = 1; /* create uuid v1 by default */
	while ((ch = getopt(argc, argv, "1rn:o:")) != -1)
		switch (ch) {
		case '1':
			iterate = 1;
			break;
		case 'r':
			version = 4;
			break;
		case 'n':
			if (count > 0)
				usage();
			count = strtol(optarg, &p, 10);
			if (*p != 0 || count < 1)
				usage();
			break;
		case 'o':
			if (fp != stdout)
				errx(1, "multiple output files not allowed");
			fp = fopen(optarg, "w");
			if (fp == NULL)
				err(1, "fopen");
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc)
		usage();

	caph_cache_catpages();
	if (caph_limit_stdio() < 0)
		err(1, "Unable to limit stdio");
	if (caph_enter() < 0)
		err(1, "Unable to enter capability mode");

	if (count == -1)
		count = 1;

	store = (uuid_t *)malloc(sizeof(uuid_t) * count);
	if (store == NULL)
		err(1, "malloc()");

	if (!iterate) {
		/* Get them all in a single batch */
		if (version == 1) {
			if (uuidgen(store, count) != 0)
				err(1, "uuidgen()");
		} else if (version == 4) {
			if (uuidgen_v4(store, count) != 0)
				err(1, "uuidgen_v4()");
		} else {
			err(1, "unsupported version");
		}
	} else {
		uuid = store;
		for (i = 0; i < count; i++) {
			if (version == 1) {
				if (uuidgen(uuid++, 1) != 0)
					err(1, "uuidgen()");
			} else if (version == 4) {
				if (uuidgen_v4(uuid++, 1) != 0)
					err(1, "uuidgen_v4()");
			} else {
				err(1, "unsupported version");
			}
		}
	}

	uuid = store;
	while (count--) {
		uuid_to_string(uuid++, &p, &status);
		if (status != uuid_s_ok)
			err(1, "cannot stringify a UUID");
		fprintf(fp, "%s\n", p);
		free(p);
	}

	free(store);
	if (fp != stdout)
		fclose(fp);
	return (0);
}
