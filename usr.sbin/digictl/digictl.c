/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
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
 *
 * $FreeBSD$
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/digiio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int
usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-d debug] [-ir] device...\n", prog);
	return (EX_USAGE);
}

int
main(int argc, char **argv)
{
	char namedata[256], *name = namedata;
	const char *prog;
	enum digi_model model;
	int ch, debug, fd, i, res;
	int dflag, iflag, rflag;

	if ((prog = strrchr(argv[0], '/')) == NULL)
		prog = argv[0];
	else
		prog++;

	dflag = iflag = rflag = 0;
	while ((ch = getopt(argc, argv, "d:ir")) != -1)
		switch (ch) {
		case 'd':
			dflag = 1;
			debug = atoi(optarg);
			break;
		case 'i':
			iflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		default:
			return usage(prog);
		}

	if (argc < optind)
		return usage(prog);

	res = 0;
	for (i = optind; i < argc; i++) {
		if ((fd = open(argv[i], O_RDONLY)) == -1) {
			fprintf(stderr, "%s: %s: open: %s\n", prog, argv[i],
			    strerror(errno));
			res++;
			continue;
		}

		if (dflag && ioctl(fd, DIGIIO_DEBUG, &debug) != 0) {
			fprintf(stderr, "%s: %s: debug: %s\n",
			    prog, argv[i], strerror(errno));
			res++;
		}

		if (iflag) {
			if (ioctl(fd, DIGIIO_MODEL, &model) != 0) {
				fprintf(stderr, "%s: %s: model: %s\n",
				    prog, argv[i], strerror(errno));
				res++;
			} else if (ioctl(fd, DIGIIO_IDENT, &name) != 0) {
				fprintf(stderr, "%s: %s: ident: %s\n",
				    prog, argv[i], strerror(errno));
				res++;
			} else
				printf("%s: %s (type %d)\n",
				    argv[i], name, (int)model);
		}

		if (rflag && ioctl(fd, DIGIIO_REINIT) != 0) {
			fprintf(stderr, "%s: %s: reinit: %s\n",
			    prog, argv[i], strerror(errno));
			res++;
		}

		close(fd);
	}

	return (res);
}
