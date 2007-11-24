/*-
 * Copyright (c) 1997 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "usage: kldunload [-fv] -i id\n");
	fprintf(stderr, "       kldunload [-fv] [-n] name\n");
	exit(1);
}

int
main(int argc, char** argv)
{
	struct kld_file_stat stat;
	int c;
	int verbose = 0;
	int fileid = 0;
	int force = LINKER_UNLOAD_NORMAL;
	char *filename = NULL;

	while ((c = getopt(argc, argv, "fi:n:v")) != -1) {
		switch (c) {
		case 'f':
			force = LINKER_UNLOAD_FORCE;
			break;
		case 'i':
			fileid = atoi(optarg);
			if (!fileid)
				errx(1, "Invalid ID %s", optarg);
			break;
		case 'n':
			filename = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (fileid == 0 && filename == NULL && (argc == 1)) {
		filename = *argv;
		argc--;
	}

	if (argc != 0 || (fileid != 0 && filename != NULL))
		usage();

	if (fileid == 0 && filename == NULL)
		usage();

	if (filename != NULL) {
		if ((fileid = kldfind(filename)) < 0)
			err(1, "can't find file %s", filename);
	}

	if (verbose) {
		stat.version = sizeof stat;
		if (kldstat(fileid, &stat) < 0)
			err(1, "can't stat file");
		printf("Unloading %s, id=%d\n", stat.name, fileid);
	}

	if (kldunloadf(fileid, force) < 0)
		err(1, "can't unload file");

	return 0;
}
