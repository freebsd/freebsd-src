/*-
 * Copyright (c) 1992 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

static void
usage()
{
	fprintf(stderr,
	"usage: comcontrol <filename> [dtrwait <n>] [drainwait <n>]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int	fd;
	int     res = 0;
	int     dtrwait = -1, drainwait = -1;

	if (argc < 2)
		usage();

	fd = open(argv[1], O_RDONLY|O_NONBLOCK, 0);
	if (fd < 0) {
		warn("couldn't open file %s", argv[1]);
		return 1;
	}

	if (argc == 2) {
		if (ioctl(fd, TIOCMGDTRWAIT, &dtrwait) < 0) {
			res = 1;
			warn("TIOCMGDTRWAIT");
		}
		if (ioctl(fd, TIOCGDRAINWAIT, &drainwait) < 0) {
			res = 1;
			warn("TIOCGDRAINWAIT");
		}
		printf("dtrwait %d drainwait %d\n", dtrwait, drainwait);
	} else {
		while (argv[2] != NULL) {
			if (!strcmp(argv[2],"dtrwait")) {
				if (dtrwait >= 0)
					usage();
				if (argv[3] == NULL || !isdigit(argv[3][0]))
					usage();
				dtrwait = atoi(argv[3]);
				argv += 2;
			} else if (!strcmp(argv[2],"drainwait")) {
				if (drainwait >= 0)
					usage();
				if (argv[3] == NULL || !isdigit(argv[3][0]))
					usage();
				drainwait = atoi(argv[3]);
				argv += 2;
			} else
				usage();
		}
		if (dtrwait >= 0) {
			if (ioctl(fd, TIOCMSDTRWAIT, &dtrwait) < 0) {
				res = 1;
				warn("TIOCMSDTRWAIT");
			}
		}
		if (drainwait >= 0) {
			if (ioctl(fd, TIOCSDRAINWAIT, &drainwait) < 0) {
				res = 1;
				warn("TIOCSDRAINWAIT");
			}
		}
	}

	close(fd);
	return res;
}
