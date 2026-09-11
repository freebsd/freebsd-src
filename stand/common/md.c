/*-
 * Copyright (c) 2009 Marcel Moolenaar
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

#include <stand.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/queue.h>
#include <sys/stdarg.h>

#include "bootstrap.h"

#define	MD_BLOCK_SIZE	512
#define	MD_FLAG_MASK	MD_FLAG_KERNEL

struct md_info {
	STAILQ_ENTRY(md_info) md_link;
	void		*md_image;
	size_t		 md_size;
	unsigned int	 md_flags;
	int		 md_unit;
};

static STAILQ_HEAD(, md_info) md_list = STAILQ_HEAD_INITIALIZER(md_list);
static int md_next_unit;

static int md_init(void);
static int md_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int md_open(struct open_file *, ...);
static int md_close(struct open_file *);
static int md_print(int);

struct devsw md_dev = {
	.dv_name = "md",
	.dv_type = DEVT_DISK,
	.dv_init = md_init,
	.dv_strategy = md_strategy,
	.dv_open = md_open,
	.dv_close = md_close,
	.dv_ioctl = noioctl,
	.dv_print = md_print,
	.dv_cleanup = nullsys,
};

/*
 * The kernel can accept a MD inline in the metadata, or out-of-line via
 * hints, like when memory disks are passed to us in, for example, UEFI.
 */
void
md_export_to_kernel(uint64_t start, uint64_t len)
{
	char key[32], value[32];
	static int unit = 0;	/* Note: loader unit and kernel unit may differ */

	snprintf(key, sizeof(key), "hint.md.%d.physaddr", unit);
	snprintf(value, sizeof(value), "0x%016jx", (uintmax_t)start);
	setenv(key, value, 1);
	snprintf(key, sizeof(key), "hint.md.%d.len", unit);
	snprintf(value, sizeof(value), "%jd", (uintmax_t)len);
	setenv(key, value, 1);
	unit++;
}

static struct md_info *
md_get_info(int unit)
{
	struct md_info *md;

	STAILQ_FOREACH(md, &md_list, md_link) {
		if (md->md_unit == unit)
			return (md);
	}
	return (NULL);
}

int
md_register(void *image, size_t size, unsigned int flags)
{
	struct md_info *md;

	if (image == NULL || size == 0 || size % MD_BLOCK_SIZE != 0 ||
	    (flags & ~MD_FLAG_MASK) != 0) {
		errno = EINVAL;
		return (-1);
	}
	md = malloc(sizeof(*md));
	if (md == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	md->md_image = image;
	md->md_size = size;
	md->md_flags = flags;
	md->md_unit = md_next_unit++;
	STAILQ_INSERT_TAIL(&md_list, md, md_link);

	if ((flags & MD_FLAG_KERNEL) != 0)
		md_export_to_kernel((uintptr_t)image, size);
	return (md->md_unit);
}

#ifdef MD_IMAGE_SIZE

#if (MD_IMAGE_SIZE == 0 || MD_IMAGE_SIZE % MD_BLOCK_SIZE)
#error Image size must be a multiple of 512.
#endif

/*
 * Preloaded image gets put here.
 * Applications that patch the object with the image can determine
 * the size looking at the start and end markers (strings),
 * so we want them contiguous.
 */
static struct {
	u_char start[MD_IMAGE_SIZE];
	u_char end[128];
} md_image = {
	.start = "MFS Filesystem goes here",
	.end = "MFS Filesystem had better STOP here",
};
#endif

static int
md_init(void)
{

#ifdef MD_IMAGE_SIZE
	if (md_register(md_image.start, MD_IMAGE_SIZE, 0) < 0)
		return (errno);
#endif
	return (0);
}

static int
md_strategy(void *devdata, int rw, daddr_t blk, size_t size,
    char *buf, size_t *rsize)
{
	struct devdesc *dev = (struct devdesc *)devdata;
	struct md_info *md;
	size_t ofs;

	md = md_get_info(dev->d_unit);
	if (md == NULL)
		return (ENXIO);

	if (blk < 0 || (uintmax_t)blk >= md->md_size / MD_BLOCK_SIZE)
		return (EIO);

	if (size % MD_BLOCK_SIZE)
		return (EIO);

	ofs = blk * MD_BLOCK_SIZE;
	if (size > md->md_size - ofs)
		size = md->md_size - ofs;

	if (rsize != NULL)
		*rsize = size;

	switch (rw & F_MASK) {
	case F_READ:
		bcopy((char *)md->md_image + ofs, buf, size);
		return (0);
	case F_WRITE:
		bcopy(buf, (char *)md->md_image + ofs, size);
		return (0);
	}

	return (ENODEV);
}

static int
md_open(struct open_file *f, ...)
{
	va_list ap;
	struct devdesc *dev;

	va_start(ap, f);
	dev = va_arg(ap, struct devdesc *);
	va_end(ap);

	if (md_get_info(dev->d_unit) == NULL)
		return (ENXIO);

	return (0);
}

static int
md_close(struct open_file *f)
{
	struct devdesc *dev;

	dev = (struct devdesc *)(f->f_devdata);
	return (md_get_info(dev->d_unit) == NULL ? ENXIO : 0);
}

static int
md_print(int verbose)
{
	struct md_info *md;
	int ret;

	if (STAILQ_EMPTY(&md_list))
		return (0);
	printf("%s devices:", md_dev.dv_name);
	if ((ret = pager_output("\n")) != 0)
		return (ret);

	STAILQ_FOREACH(md, &md_list, md_link) {
		printf("    %s%d:    %ju X %u blocks", md_dev.dv_name,
		    md->md_unit, (uintmax_t)(md->md_size / MD_BLOCK_SIZE),
		    MD_BLOCK_SIZE);
		if ((ret = pager_output("\n")) != 0)
			return (ret);
	}
	return (0);
}
