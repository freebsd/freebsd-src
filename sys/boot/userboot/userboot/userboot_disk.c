/*-
 * Copyright (c) 2011 Google, Inc.
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

/*
 * Userboot disk image handling.
 */

#include <sys/disk.h>
#include <stand.h>
#include <stdarg.h>
#include <bootstrap.h>

#include "disk.h"
#include "libuserboot.h"

int userboot_disk_maxunit = 0;

static int	userdisk_init(void);
static int	userdisk_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	userdisk_open(struct open_file *f, ...);
static int	userdisk_close(struct open_file *f);
static int	userdisk_ioctl(struct open_file *f, u_long cmd, void *data);
static void	userdisk_print(int verbose);

struct devsw userboot_disk = {
	"disk",
	DEVT_DISK,
	userdisk_init,
	userdisk_strategy,
	userdisk_open,
	userdisk_close,
	userdisk_ioctl,
	userdisk_print,
	NULL
};

/*
 * Nothing to do here.
 */
static int
userdisk_init(void)
{

	return(0);
}

/*
 * Print information about disks
 */
static void
userdisk_print(int verbose)
{
	struct disk_devdesc dev;
	char line[80];
	off_t mediasize;
	u_int sectorsize;
	int i;

	for (i = 0; i < userboot_disk_maxunit; i++) {
		sprintf(line, "    disk%d:   Guest drive image\n", i);
		pager_output(line);
		if (CALLBACK(diskioctl, i, DIOCGSECTORSIZE, &sectorsize) != 0 ||
		    CALLBACK(diskioctl, i, DIOCGMEDIASIZE, &mediasize) != 0)
			continue;
		dev.d_dev = &userboot_disk;
		dev.d_unit = i;
		dev.d_slice = -1;
		dev.d_partition = -1;
		if (disk_open(&dev, mediasize, sectorsize) == 0) {
			sprintf(line, "    disk%d", i);
			disk_print(&dev, line, verbose);
			disk_close(&dev);
		}
	}
}

/*
 * Attempt to open the disk described by (dev) for use by (f).
 */
static int
userdisk_open(struct open_file *f, ...)
{
	va_list			ap;
	struct disk_devdesc	*dev;
	u_int sectorsize;
	off_t mediasize;
	int rc;

	va_start(ap, f);
	dev = va_arg(ap, struct disk_devdesc *);
	va_end(ap);

	if (dev->d_unit < 0 || dev->d_unit >= userboot_disk_maxunit)
		return (EIO);

	rc = CALLBACK(diskioctl, dev->d_unit, DIOCGSECTORSIZE, &sectorsize);
	if (rc != 0)
		return (rc);
	rc = CALLBACK(diskioctl, dev->d_unit, DIOCGMEDIASIZE, &mediasize);
	if (rc != 0)
		return (rc);
	return (disk_open(dev, mediasize, sectorsize));
}

static int
userdisk_close(struct open_file *f)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)f->f_devdata;
	return (disk_close(dev));
}

static int
userdisk_strategy(void *devdata, int rw, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
	struct disk_devdesc *dev = devdata;
	uint64_t	off;
	size_t		resid;
	int		rc;

	if (rw == F_WRITE)
		return (EROFS);
	if (rw != F_READ)
		return (EINVAL);
	if (rsize)
		*rsize = 0;
	off = (dblk + dev->d_offset) * DISK_SECSIZE;
	rc = CALLBACK(diskread, dev->d_unit, off, buf, size, &resid);
	if (rc)
		return (rc);
	if (rsize)
		*rsize = size - resid;
	return (0);
}

static int
userdisk_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)f->f_devdata;
	return (CALLBACK(diskioctl, dev->d_unit, cmd, data));
}
