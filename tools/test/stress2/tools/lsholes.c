/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct stat st;
	off_t data, hole, pos;
	long mx;
	int fd, n;
	char *name;

        if (argc != 2) {
                fprintf(stderr, "Usage: %s <file>\n", argv[0]);
                exit(1);
        }

        name = argv[1];
        if ((fd = open(name, O_RDONLY)) == -1)
                err(1, "open(%s)", name);
        if (fstat(fd, &st))
                err(1, "fstat()");
	if ((mx = fpathconf(fd, _PC_MIN_HOLE_SIZE)) == -1)
		err(1, "fpathconf()");
	fprintf(stderr, "file \"%s\" size = %jd, _PC_MIN_HOLE_SIZE = %ld\n",
	    name, (intmax_t)st.st_size, mx);
	n = 1;
	pos = 0;
	while (pos < st.st_size) {
		if ((hole = lseek(fd, pos, SEEK_HOLE)) == -1)
			err(1, "lseek(SEEK_HOLE)");
		if ((data = lseek(fd, hole, SEEK_DATA)) == -1) {
			if (errno == ENXIO) {
				if (hole == st.st_size)
					break;
				fprintf(stderr,
				    "No data after hole @ %jd\n",
				    (intmax_t)hole);
				break;
			}
			err(1, "lseek(SEEK_DATA)");
		}
		pos = data;
		printf("hole #%d @ %jd (0x%jx), size=%jd (0x%jx)\n",
		    n, (intmax_t)hole, (intmax_t)hole, (intmax_t)(data - hole),
		    (intmax_t)(data - hole));
		n++;
        }
        close(fd);
	if (hole != st.st_size)
		errx(1, "No implicit hole at EOF");
}
