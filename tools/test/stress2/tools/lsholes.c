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
	long mn;
	intmax_t siz;
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
	if ((mn = fpathconf(fd, _PC_MIN_HOLE_SIZE)) == -1)
		err(1, "fpathconf()");
	fprintf(stderr, "Min hole size is %ld, file size is %jd.\n",
	    mn, (intmax_t)st.st_size);
	n = 1;
	pos = 0;

	while (pos < st.st_size) {
		hole = lseek(fd, pos, SEEK_HOLE);
		if (hole == -1 && errno != ENXIO)
			err(1, "lseek(SEEK_HOLE)");
		data = lseek(fd, pos, SEEK_DATA);
		if (data == -1 && errno != ENXIO)
			err(1, "lseek(SEEK_data)");

		if (hole >= 0 && data >= 0 && hole > data) {
			siz = hole - data;
			printf("data #%d @ %ld, size=%jd)\n",
			    n, (intmax_t)data, siz);
			n++;
			pos += siz;
		}
		if (hole >= 0 && data >= 0 && hole < data) {
			siz = data - hole;
			printf("hole #%d @ %ld, size=%jd\n",
			    n, (intmax_t)hole, siz);
			n++;
			pos += siz;
		}
		if (hole >= 0 && data == -1) {
			siz = st.st_size - hole;
			printf("hole #%d @ %ld, size=%jd\n",
			    n, (intmax_t)hole, siz);
			n++;
			pos += siz;
		}
        }
	if (hole == st.st_size) {
		/* EOF */
		printf("hole #%d @ %ld, size=%jd\n",
		    n, (intmax_t)hole, 0L);
	}
        close(fd);
}
