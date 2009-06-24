/*-
 * Copyright (c) 2009 Ulf Lilleengen
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

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

struct  filestat {
	long	fsid;
	long	fileid;
	mode_t	mode;
	u_long	size;
	dev_t	rdev;
};

int
main(int argc, char **argv)
{
	struct filestat *fs_buf;
	struct filestat *fsp;
	size_t size, numentries, i;

	if (sysctlbyname("kern.fileinfo", NULL, &size, NULL, 0) == -1) {
		fprintf(stderr, "error getting sysctl\n");
		return (0);
	}
	fs_buf = malloc(size);
	if (fs_buf == NULL) {
		printf("OOPS\n");
		return (-1);
	}
	printf("Data size: %d\n", size);
	numentries = size / sizeof(struct filestat);
	printf("Data entries: %d\n", numentries);
	if (sysctlbyname("kern.fileinfo", fs_buf, &size, NULL, 0) == -1) {
		fprintf(stderr, "error getting sysctl\n");
		return (0);
	}
	fsp = fs_buf;
	for (i = 0; i < numentries; i++) {
		printf("FSID: %ld fileid %ld Size: %lu\n", fsp->fsid, fsp->fileid, fsp->size);

		fsp++;
	}
}
