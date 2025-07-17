/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>

#undef NDEBUG
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libder.h>

#include "fuzzers.h"

#define	LARGE_SIZE	(1024 * 64)

static const uint8_t empty_seq[] = { BT_SEQUENCE, 0x00 };
static const uint8_t long_size[21] = { BT_OCTETSTRING, 0x83, 0x00, 0x00, 0x10 };

/* 64k */
#define	LARGE_SIZE_ENCODING	0x83, 0x01, 0x00, 0x00
static const uint8_t large_octet[LARGE_SIZE + 5] = { BT_OCTETSTRING, LARGE_SIZE_ENCODING };

#define	VARLEN_SEQ	BT_OCTETSTRING, 0x04, 0x01, 0x02, 0x03, 0x04
#define	VARLEN_CHILDREN	VARLEN_SEQ, VARLEN_SEQ, VARLEN_SEQ
static const uint8_t varlen[] = { BT_SEQUENCE, 0x80,
    VARLEN_CHILDREN, 0x00, 0x00 };

#define	BITSTRING1	BT_BITSTRING, 0x04, 0x03, 0xc0, 0xc0, 0xcc
#define	BITSTRING2	BT_BITSTRING, 0x04, 0x05, 0xdd, 0xdd, 0xff
static const uint8_t constructed_bitstring[] = { 0x20 | BT_BITSTRING,
    2 * 6, BITSTRING1, BITSTRING2 };

#define	FUZZER_SEED(seq)	{ #seq, sizeof(seq), seq }
static const struct seed {
	const char	*seed_name;
	size_t		 seed_seqsz;
	const uint8_t	*seed_seq;
} seeds[] = {
	FUZZER_SEED(empty_seq),
	FUZZER_SEED(long_size),
	FUZZER_SEED(large_octet),
	FUZZER_SEED(varlen),
	FUZZER_SEED(constructed_bitstring),
};

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-H] <corpus-dir>\n", getprogname());
	exit(1);
}

static void
write_one(const struct fuzz_params *params, const struct seed *seed, int dirfd,
    bool striphdr)
{
	char *name;
	int fd = -1;

	assert(asprintf(&name, "base_%d_%d_%d_%s", params->type,
	    params->buftype, params->strict, seed->seed_name) != -1);

	fd = openat(dirfd, name, O_RDWR | O_TRUNC | O_CREAT, 0644);
	assert(fd != -1);

	/*
	 * Write our params + seed; if we're stripping the header we won't have
	 * the full params, but we'll still have our signal byte for strict
	 * mode.
	 */
	if (!striphdr)
		assert(write(fd, &params,  sizeof(params)) == sizeof(params));
	else
		assert(write(fd, &params->strict, sizeof(params->strict)) == sizeof(params->strict));

	assert(write(fd, seed->seed_seq, seed->seed_seqsz) == seed->seed_seqsz);

	free(name);
	close(fd);
}

int
main(int argc, char *argv[])
{
	struct fuzz_params params;
	const struct seed *seed;
	const char *seed_dir;
	int dirfd = -1;
	bool striphdr = false;

	if (argc < 2 || argc > 3)
		usage();

	if (argc == 3 && strcmp(argv[1], "-H") != 0)
		usage();

	striphdr = argc == 3;
	seed_dir = argv[argc - 1];

	dirfd = open(seed_dir, O_SEARCH);
	if (dirfd == -1)
		err(1, "%s: open", seed_dir);

	memset(&params, 0, sizeof(params));

	for (int type = 0; type < STREAM_END; type++) {
		params.type = type;

		for (int buffered = 0; buffered < BUFFER_END; buffered++) {
			params.buftype = buffered;

			for (uint8_t strict = 0; strict < 2; strict++) {
				params.strict = strict;

				for (size_t i = 0; i < nitems(seeds); i++) {
					seed = &seeds[i];

					write_one(&params, seed, dirfd, striphdr);
				}
			}

			if (type != STREAM_FILE)
				break;
		}
	}

	close(dirfd);
}
