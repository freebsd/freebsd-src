/*-
 * Copyright (c) 2008 Semihalf, Rafal Jaworowski
 * Copyright (c) 2009 Semihalf, Piotr Ziecik
 * Copyright (c) 2012 Andrey V. Elsukov <ae@FreeBSD.org>
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
 */

/*
 * Block storage I/O routines for U-Boot
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disk.h>
#include <machine/stdarg.h>
#include <stand.h>

#include "api_public.h"
#include "bootstrap.h"
#include "disk.h"
#include "glue.h"
#include "libuboot.h"

#define stor_printf(fmt, args...) do {			\
    printf("%s%d: ", dev->d_dev->dv_name, dev->d_unit);	\
    printf(fmt, ##args);				\
} while (0)

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

static struct {
	int		opened;	/* device is opened */
	int		handle;	/* storage device handle */
	int		type;	/* storage type */
	off_t		blocks;	/* block count */
	u_int		bsize;	/* block size */
} stor_info[UB_MAX_DEV];

#define	SI(dev)		(stor_info[(dev)->d_unit])

static int stor_info_no = 0;
static int stor_opendev(struct disk_devdesc *);
static int stor_readdev(struct disk_devdesc *, daddr_t, size_t, char *);

/* devsw I/F */
static int stor_init(void);
static int stor_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int stor_open(struct open_file *, ...);
static int stor_close(struct open_file *);
static int stor_ioctl(struct open_file *f, u_long cmd, void *data);
static void stor_print(int);
static void stor_cleanup(void);

struct devsw uboot_storage = {
	"disk",
	DEVT_DISK,
	stor_init,
	stor_strategy,
	stor_open,
	stor_close,
	stor_ioctl,
	stor_print,
	stor_cleanup
};

static int
stor_init(void)
{
	struct device_info *di;
	int i;

	if (devs_no == 0) {
		printf("No U-Boot devices! Really enumerated?\n");
		return (-1);
	}

	for (i = 0; i < devs_no; i++) {
		di = ub_dev_get(i);
		if ((di != NULL) && (di->type & DEV_TYP_STOR)) {
			if (stor_info_no >= UB_MAX_DEV) {
				printf("Too many storage devices: %d\n",
				    stor_info_no);
				return (-1);
			}
			stor_info[stor_info_no].handle = i;
			stor_info[stor_info_no].opened = 0;
			stor_info[stor_info_no].type = di->type;
			stor_info[stor_info_no].blocks =
			    di->di_stor.block_count;
			stor_info[stor_info_no].bsize =
			    di->di_stor.block_size;
			stor_info_no++;
		}
	}

	if (!stor_info_no) {
		debugf("No storage devices\n");
		return (-1);
	}

	debugf("storage devices found: %d\n", stor_info_no);
	return (0);
}

static void
stor_cleanup(void)
{
	int i;

	for (i = 0; i < stor_info_no; i++)
		if (stor_info[i].opened > 0)
			ub_dev_close(stor_info[i].handle);
	disk_cleanup(&uboot_storage);
}

static int
stor_strategy(void *devdata, int rw, daddr_t blk, size_t size, char *buf,
    size_t *rsize)
{
	struct disk_devdesc *dev = (struct disk_devdesc *)devdata;
	daddr_t bcount;
	int err;

	if (rw != F_READ) {
		stor_printf("write attempt, operation not supported!\n");
		return (EROFS);
	}

	if (size % SI(dev).bsize) {
		stor_printf("size=%d not multiple of device block size=%d\n",
		    size, SI(dev).bsize);
		return (EIO);
	}
	bcount = size / SI(dev).bsize;
	if (rsize)
		*rsize = 0;

	err = stor_readdev(dev, blk + dev->d_offset, bcount, buf);
	if (!err && rsize)
		*rsize = size;

	return (err);
}

static int
stor_open(struct open_file *f, ...)
{
	va_list ap;
	struct disk_devdesc *dev;

	va_start(ap, f);
	dev = va_arg(ap, struct disk_devdesc *);
	va_end(ap);

	return (stor_opendev(dev));
}

static int
stor_opendev(struct disk_devdesc *dev)
{
	int err;

	if (dev->d_unit < 0 || dev->d_unit >= stor_info_no)
		return (EIO);

	if (SI(dev).opened == 0) {
		err = ub_dev_open(SI(dev).handle);
		if (err != 0) {
			stor_printf("device open failed with error=%d, "
			    "handle=%d\n", err, SI(dev).handle);
			return (ENXIO);
		}
		SI(dev).opened++;
	}
	return (disk_open(dev, SI(dev).blocks * SI(dev).bsize,
	    SI(dev).bsize, 0));
}

static int
stor_close(struct open_file *f)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)(f->f_devdata);
	return (disk_close(dev));
}

static int
stor_readdev(struct disk_devdesc *dev, daddr_t blk, size_t size, char *buf)
{
	lbasize_t real_size;
	int err;

	debugf("reading blk=%d size=%d @ 0x%08x\n", (int)blk, size, (uint32_t)buf);

	err = ub_dev_read(SI(dev).handle, buf, size, blk, &real_size);
	if (err != 0) {
		stor_printf("read failed, error=%d\n", err);
		return (EIO);
	}

	if (real_size != size) {
		stor_printf("real size != size\n");
		err = EIO;
	}

	return (err);
}

static void
stor_print(int verbose)
{
	struct disk_devdesc dev;
	static char line[80];
	int i;

	for (i = 0; i < stor_info_no; i++) {
		dev.d_dev = &uboot_storage;
		dev.d_unit = i;
		dev.d_slice = -1;
		dev.d_partition = -1;
		sprintf(line, "\tdisk%d (%s)\n", i,
		    ub_stor_type(SI(&dev).type));
		pager_output(line);
		if (stor_opendev(&dev) == 0) {
			sprintf(line, "\tdisk%d", i);
			disk_print(&dev, line, verbose);
			disk_close(&dev);
		}
	}
}

static int
stor_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)f->f_devdata;
	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = SI(dev).bsize;
		break;
	case DIOCGMEDIASIZE:
		*(off_t *)data = SI(dev).bsize * SI(dev).blocks;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

