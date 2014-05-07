/*-
 * Copyright (c) 2014 Juniper Networks, Inc.
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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "image.h"
#include "mkimg.h"

#define	BUFFER_SIZE	(1024*1024)

int
image_copyin(lba_t blk, int fd, uint64_t *sizep)
{
	char *buffer;
	uint64_t bytesize;
	ssize_t bcnt, rdsz;
	int error, partial;

	assert(BUFFER_SIZE % secsz == 0);

	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return (ENOMEM);
	bytesize = 0;
	partial = 0;
	while (1) {
		rdsz = read(fd, buffer, BUFFER_SIZE);
		if (rdsz <= 0) {
			error = (rdsz < 0) ? errno : 0;
			break;
		}
		if (partial)
			abort();
		bytesize += rdsz;
		bcnt = (rdsz + secsz - 1) / secsz;
		error = image_write(blk, buffer, bcnt);
		if (error)
			break;
		blk += bcnt;
		partial = (bcnt * secsz != rdsz) ? 1 : 0;
	}
	free(buffer);
	if (sizep != NULL)
		*sizep = bytesize;
	return (error);
}

int
image_set_size(lba_t blk __unused)
{

	/* TODO */
	return (0);
}

int
image_write(lba_t blk __unused, void *buf __unused, ssize_t len __unused)
{

	/* TODO */
	return (0);
}
