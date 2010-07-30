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
#include <limits.h>
#include <inttypes.h>
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

/*
 * The size of a sector is context specific (i.e. determined by the
 * media). But when users enter a value with a SI unit, they really
 * mean the byte-size or byte-offset and not the size or offset in
 * sectors. We should map the byte-oriented value into a sector-oriented
 * value when we already know the sector size in bytes. At this time
 * we can use g_parse_lba() function. It converts user specified
 * value into sectors with following conditions:
 * o  Sectors size taken as argument from caller.
 * o  When no SI unit is specified the value is in sectors.
 * o  With an SI unit the value is in bytes.
 * o  The 'b' suffix forces byte interpretation and the 's'
 *    suffix forces sector interpretation.
 *
 * Thus:
 * o  2 and 2s mean 2 sectors, and 2b means 2 bytes.
 * o  4k and 4kb mean 4096 bytes, and 4ks means 4096 sectors.
 *
 */
int
g_parse_lba(const char *lbastr, unsigned sectorsize, off_t *sectors)
{
	off_t number, mult, unit;
	char *s;

	assert(lbastr != NULL);
	assert(sectorsize > 0);
	assert(sectors != NULL);

	number = (off_t)strtoimax(lbastr, &s, 0);
	if (s == lbastr || number < 0)
		return (EINVAL);

	mult = 1;
	unit = sectorsize;
	if (*s == '\0')
		goto done;
	switch (*s) {
	case 'e': case 'E':
		mult *= 1024;
		/* FALLTHROUGH */
	case 'p': case 'P':
		mult *= 1024;
		/* FALLTHROUGH */
	case 't': case 'T':
		mult *= 1024;
		/* FALLTHROUGH */
	case 'g': case 'G':
		mult *= 1024;
		/* FALLTHROUGH */
	case 'm': case 'M':
		mult *= 1024;
		/* FALLTHROUGH */
	case 'k': case 'K':
		mult *= 1024;
		break;
	default:
		goto sfx;
	}
	unit = 1;	/* bytes */
	s++;
	if (*s == '\0')
		goto done;
sfx:
	switch (*s) {
	case 's': case 'S':
		unit = sectorsize;	/* sector */
		break;
	case 'b': case 'B':
		unit = 1;		/* bytes */
		break;
	default:
		return (EINVAL);
	}
	s++;
	if (*s != '\0')
		return (EINVAL);
done:
	if ((OFF_MAX / unit) < mult || (OFF_MAX / mult / unit) < number)
		return (ERANGE);
	number *= mult * unit;
	if (number % sectorsize)
		return (EINVAL);
	number /= sectorsize;
	*sectors = number;
	return (0);
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
	(void)ioctl(fd, DIOCGFLUSH, NULL);
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
	(void)ioctl(fd, DIOCGFLUSH, NULL);
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

static void *
gctl_get_param(struct gctl_req *req, size_t len, const char *pfmt, va_list ap)
{
	struct gctl_req_arg *argp;
	char param[256];
	void *p;
	unsigned i;

	vsnprintf(param, sizeof(param), pfmt, ap);
	for (i = 0; i < req->narg; i++) {
		argp = &req->arg[i];
		if (strcmp(param, argp->name))
			continue;
		if (!(argp->flag & GCTL_PARAM_RD))
			continue;
		p = argp->value;
		if (len == 0) {
			/* We are looking for a string. */
			if (argp->len < 1) {
				fprintf(stderr, "No length argument (%s).\n",
				    param);
				abort();
			}
			if (((char *)p)[argp->len - 1] != '\0') {
				fprintf(stderr, "Unterminated argument (%s).\n",
				    param);
				abort();
			}
		} else if ((int)len != argp->len) {
			fprintf(stderr, "Wrong length %s argument.\n", param);
			abort();
		}
		return (p);
	}
	fprintf(stderr, "No such argument (%s).\n", param);
	abort();
}

int
gctl_get_int(struct gctl_req *req, const char *pfmt, ...)
{
	int *p;
	va_list ap;

	va_start(ap, pfmt);
	p = gctl_get_param(req, sizeof(int), pfmt, ap);
	va_end(ap);
	return (*p);
}

intmax_t
gctl_get_intmax(struct gctl_req *req, const char *pfmt, ...)
{
	intmax_t *p;
	va_list ap;

	va_start(ap, pfmt);
	p = gctl_get_param(req, sizeof(intmax_t), pfmt, ap);
	va_end(ap);
	return (*p);
}

const char *
gctl_get_ascii(struct gctl_req *req, const char *pfmt, ...)
{
	const char *p;
	va_list ap;

	va_start(ap, pfmt);
	p = gctl_get_param(req, 0, pfmt, ap);
	va_end(ap);
	return (p);
}

int
gctl_change_param(struct gctl_req *req, const char *name, int len,
    const void *value)
{
	struct gctl_req_arg *ap;
	unsigned i;

	if (req == NULL || req->error != NULL)
		return (EDOOFUS);
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(ap->name, name) != 0)
			continue;
		ap->value = __DECONST(void *, value);
		if (len >= 0) {
			ap->flag &= ~GCTL_PARAM_ASCII;
			ap->len = len;
		} else if (len < 0) {
			ap->flag |= GCTL_PARAM_ASCII;
			ap->len = strlen(value) + 1;
		}
		return (0);
	}
	return (ENOENT);
}

int
gctl_delete_param(struct gctl_req *req, const char *name)
{
	struct gctl_req_arg *ap;
	unsigned int i;

	if (req == NULL || req->error != NULL)
		return (EDOOFUS);

	i = 0;
	while (i < req->narg) {
		ap = &req->arg[i];
		if (strcmp(ap->name, name) == 0)
			break;
		i++;
	}
	if (i == req->narg)
		return (ENOENT);

	free(ap->name);
	req->narg--;
	while (i < req->narg) {
		req->arg[i] = req->arg[i + 1];
		i++;
	}
	return (0);
}

int
gctl_has_param(struct gctl_req *req, const char *name)
{
	struct gctl_req_arg *ap;
	unsigned int i;

	if (req == NULL || req->error != NULL)
		return (0);

	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		if (strcmp(ap->name, name) == 0)
			return (1);
	}
	return (0);
}
