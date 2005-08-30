/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/disk.h>
#include <sys/endian.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <libgeom.h>

#include "misc/subr.h"


struct std_metadata {
	char		md_magic[16];
	uint32_t	md_version;
};

static void
std_metadata_decode(const u_char *data, struct std_metadata *md)
{

        bcopy(data, md->md_magic, sizeof(md->md_magic));
        md->md_version = le32dec(data + 16);
}

static void
pathgen(const char *name, char *path, size_t size)
{

	if (strncmp(name, _PATH_DEV, strlen(_PATH_DEV)) != 0)
		snprintf(path, size, "%s%s", _PATH_DEV, name);
	else
		strlcpy(path, name, size);
}

/*
 * Greatest Common Divisor.
 */
static unsigned
gcd(unsigned a, unsigned b)
{
	u_int c;

	while (b != 0) {
		c = a;
		a = b;
		b = (c % b);
	}
	return (a);
}

/*
 * Least Common Multiple.
 */
unsigned
g_lcm(unsigned a, unsigned b)
{

	return ((a * b) / gcd(a, b));
}

uint32_t
bitcount32(uint32_t x)
{

	x = (x & 0x55555555) + ((x & 0xaaaaaaaa) >> 1);
	x = (x & 0x33333333) + ((x & 0xcccccccc) >> 2);
	x = (x & 0x0f0f0f0f) + ((x & 0xf0f0f0f0) >> 4);
	x = (x & 0x00ff00ff) + ((x & 0xff00ff00) >> 8);
	x = (x & 0x0000ffff) + ((x & 0xffff0000) >> 16);
	return (x);
}

off_t
g_get_mediasize(const char *name)
{
	char path[MAXPATHLEN];
	off_t mediasize;
	int fd;

	pathgen(name, path, sizeof(path));
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return (0);
	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) < 0) {
		close(fd);
		return (0);
	}
	close(fd);
	return (mediasize);
}

unsigned
g_get_sectorsize(const char *name)
{
	char path[MAXPATHLEN];
	unsigned sectorsize;
	int fd;

	pathgen(name, path, sizeof(path));
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return (0);
	if (ioctl(fd, DIOCGSECTORSIZE, &sectorsize) < 0) {
		close(fd);
		return (0);
	}
	close(fd);
	return (sectorsize);
}

int
g_metadata_read(const char *name, u_char *md, size_t size, const char *magic)
{
	struct std_metadata stdmd;
	char path[MAXPATHLEN];
	unsigned sectorsize;
	off_t mediasize;
	u_char *sector;
	int error, fd;

	pathgen(name, path, sizeof(path));
	sector = NULL;
	error = 0;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return (errno);
	mediasize = g_get_mediasize(name);
	if (mediasize == 0) {
		error = errno;
		goto out;
	}
	sectorsize = g_get_sectorsize(name);
	if (sectorsize == 0) {
		error = errno;
		goto out;
	}
	assert(sectorsize >= size);
	sector = malloc(sectorsize);
	if (sector == NULL) {
		error = ENOMEM;
		goto out;
	}
	if (pread(fd, sector, sectorsize, mediasize - sectorsize) !=
	    (ssize_t)sectorsize) {
		error = errno;
		goto out;
	}
	if (magic != NULL) {
		std_metadata_decode(sector, &stdmd);
		if (strcmp(stdmd.md_magic, magic) != 0) {
			error = EINVAL;
			goto out;
		}
	}
	bcopy(sector, md, size);
out:
	if (sector != NULL)
		free(sector);
	close(fd);
	return (error);
}

int
g_metadata_store(const char *name, u_char *md, size_t size)
{
	char path[MAXPATHLEN];
	unsigned sectorsize;
	off_t mediasize;
	u_char *sector;
	int error, fd;

	pathgen(name, path, sizeof(path));
	sector = NULL;
	error = 0;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return (errno);
	mediasize = g_get_mediasize(name);
	if (mediasize == 0) {
		error = errno;
		goto out;
	}
	sectorsize = g_get_sectorsize(name);
	if (sectorsize == 0) {
		error = errno;
		goto out;
	}
	assert(sectorsize >= size);
	sector = malloc(sectorsize);
	if (sector == NULL) {
		error = ENOMEM;
		goto out;
	}
	bcopy(md, sector, size);
	if (pwrite(fd, sector, sectorsize, mediasize - sectorsize) !=
	    (ssize_t)sectorsize) {
		error = errno;
		goto out;
	}
out:
	if (sector != NULL)
		free(sector);
	close(fd);
	return (error);
}

int
g_metadata_clear(const char *name, const char *magic)
{
	struct std_metadata md;
	char path[MAXPATHLEN];
	unsigned sectorsize;
	off_t mediasize;
	u_char *sector;
	int error, fd;

	pathgen(name, path, sizeof(path));
	sector = NULL;
	error = 0;

	fd = open(path, O_RDWR);
	if (fd == -1)
		return (errno);
	mediasize = g_get_mediasize(name);
	if (mediasize == 0) {
		error = errno;
		goto out;
	}
	sectorsize = g_get_sectorsize(name);
	if (sectorsize == 0) {
		error = errno;
		goto out;
	}
	sector = malloc(sectorsize);
	if (sector == NULL) {
		error = ENOMEM;
		goto out;
	}
	if (magic != NULL) {
		if (pread(fd, sector, sectorsize, mediasize - sectorsize) !=
		    (ssize_t)sectorsize) {
			error = errno;
			goto out;
		}
		std_metadata_decode(sector, &md);
		if (strcmp(md.md_magic, magic) != 0) {
			error = EINVAL;
			goto out;
		}
	}
	bzero(sector, sectorsize);
	if (pwrite(fd, sector, sectorsize, mediasize - sectorsize) !=
	    (ssize_t)sectorsize) {
		error = errno;
		goto out;
	}
out:
	if (sector != NULL)
		free(sector);
	close(fd);
	return (error);
}

/*
 * Set an error message, if one does not already exist.
 */
void
gctl_error(struct gctl_req *req, const char *error, ...)
{
	va_list ap;

	if (req->error != NULL)
		return;
	va_start(ap, error);
	vasprintf(&req->error, error, ap);
	va_end(ap);
}

void *
gctl_get_param(struct gctl_req *req, const char *param, int *len)
{
	unsigned i;
	void *p;
	struct gctl_req_arg *ap;

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(param, ap->name))
			continue;
		if (!(ap->flag & GCTL_PARAM_RD))
			continue;
		p = ap->value;
		if (len != NULL)
			*len = ap->len;
		return (p);
	}
	return (NULL);
}

char const *
gctl_get_asciiparam(struct gctl_req *req, const char *param)
{
	unsigned i;
	char const *p;
	struct gctl_req_arg *ap;

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(param, ap->name))
			continue;
		if (!(ap->flag & GCTL_PARAM_RD))
			continue;
		p = ap->value;
		if (ap->len < 1) {
			gctl_error(req, "No length argument (%s)", param);
			return (NULL);
		}
		if (p[ap->len - 1] != '\0') {
			gctl_error(req, "Unterminated argument (%s)", param);
			return (NULL);
		}
		return (p);
	}
	return (NULL);
}

void *
gctl_get_paraml(struct gctl_req *req, const char *param, int len)
{
	int i;
	void *p;

	p = gctl_get_param(req, param, &i);
	if (p == NULL)
		gctl_error(req, "Missing %s argument", param);
	else if (i != len) {
		p = NULL;
		gctl_error(req, "Wrong length %s argument", param);
	}
	return (p);
}
