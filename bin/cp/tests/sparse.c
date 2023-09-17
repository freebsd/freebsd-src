/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

static bool verbose;

/*
 * Returns true if the file named by its argument is sparse, i.e. if
 * seeking to SEEK_HOLE returns a different value than seeking to
 * SEEK_END.
 */
static bool
sparse(const char *filename)
{
	off_t hole, end;
	int fd;

	if ((fd = open(filename, O_RDONLY)) < 0 ||
	    (hole = lseek(fd, 0, SEEK_HOLE)) < 0 ||
	    (end = lseek(fd, 0, SEEK_END)) < 0)
		err(1, "%s", filename);
	close(fd);
	if (end > hole) {
		if (verbose)
			printf("%s: hole at %zu\n", filename, (size_t)hole);
		return (true);
	}
	return (false);
}

static void
usage(void)
{

	fprintf(stderr, "usage: sparse [-v] file [...]\n");
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	int opt, rv;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();
	rv = EXIT_SUCCESS;
	while (argc-- > 0)
		if (!sparse(*argv++))
			rv = EXIT_FAILURE;
	exit(rv);
}
