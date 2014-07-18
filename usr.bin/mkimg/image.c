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
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "image.h"
#include "mkimg.h"

#define	BUFFER_SIZE	(1024*1024)

static char image_tmpfile[PATH_MAX];
static int image_fd = -1;
static lba_t image_size;

static void
cleanup(void)
{

	if (image_fd != -1)
		close(image_fd);
	unlink(image_tmpfile);
}

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
		partial = ((ssize_t)(bcnt * secsz) != rdsz) ? 1 : 0;
	}
	free(buffer);
	if (sizep != NULL)
		*sizep = bytesize;
	return (error);
}

int
image_copyout(int fd)
{
	int error;

	error = image_copyout_region(fd, 0, image_size);
	if (!error)
		error = image_copyout_done(fd);
	return (error);
}

int
image_copyout_done(int fd)
{
	off_t ofs;
	int error;

	ofs = lseek(fd, 0L, SEEK_CUR);
	if (ofs == -1)
		return (0);
	error = (ftruncate(fd, ofs) == -1) ? errno : 0;
	return (error);
}

int
image_copyout_region(int fd, lba_t blk, lba_t size)
{
	char *buffer;
	off_t ofs;
	size_t sz;
	ssize_t rdsz, wrsz;
	int error;

	ofs = lseek(fd, 0L, SEEK_CUR);

	blk *= secsz;
	if (lseek(image_fd, blk, SEEK_SET) != blk)
		return (errno);
	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return (errno);
	error = 0;
	size *= secsz;
	while (size > 0) {
		sz = (BUFFER_SIZE < size) ? BUFFER_SIZE : size;
		rdsz = read(image_fd, buffer, sz);
		if (rdsz <= 0) {
			error = (rdsz < 0) ? errno : 0;
			break;
		}
		wrsz = (ofs == -1) ?
		    write(fd, buffer, rdsz) :
		    sparse_write(fd, buffer, rdsz);
		if (wrsz < 0) {
			error = errno;
			break;
		}
		assert(wrsz == rdsz);
		size -= rdsz;
	}
	free(buffer);
	return (error);
}

int
image_data(lba_t blk, lba_t size)
{
	char *buffer, *p;

	blk *= secsz;
	if (lseek(image_fd, blk, SEEK_SET) != blk)
		return (1);

	size *= secsz;
	buffer = malloc(size);
	if (buffer == NULL)
		return (1);

	if (read(image_fd, buffer, size) != (ssize_t)size) {
		free(buffer);
		return (1);
	}

	p = buffer;
	while (size > 0 && *p == '\0')
		size--, p++;

	free(buffer);
	return ((size == 0) ? 0 : 1);
}

lba_t
image_get_size(void)
{

	return (image_size);
}

int
image_set_size(lba_t blk)
{

	image_size = blk;
	if (ftruncate(image_fd, blk * secsz) == -1)
		return (errno);
	return (0);
}

int
image_write(lba_t blk, void *buf, ssize_t len)
{

	blk *= secsz;
	if (lseek(image_fd, blk, SEEK_SET) != blk)
		return (errno);
	len *= secsz;
	if (sparse_write(image_fd, buf, len) != len)
		return (errno);
	return (0);
}

int
image_init(void)
{
	const char *tmpdir;

	if (atexit(cleanup) == -1)
		return (errno);
	if ((tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0')
		tmpdir = _PATH_TMP;
	snprintf(image_tmpfile, sizeof(image_tmpfile), "%s/mkimg-XXXXXX",
	    tmpdir);
	image_fd = mkstemp(image_tmpfile);
	if (image_fd == -1)
		return (errno);
	return (0);
}
