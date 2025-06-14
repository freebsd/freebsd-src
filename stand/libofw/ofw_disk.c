/*-
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Disk I/O routines using Open Firmware
 */

#include <sys/param.h>
#include <sys/stdarg.h>

#include <netinet/in.h>

#include <stand.h>
#include <sys/disk.h>

#include "disk.h"
#include "libofw.h"

static int	ofwd_init(void);
static int	ofwd_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	ofwd_open(struct open_file *f, ...);
static int	ofwd_close(struct open_file *f);
static int	ofwd_ioctl(struct open_file *f, u_long cmd, void *data);
static int	ofwd_print(int verbose);
static char *	ofwd_fmtdev(struct devdesc *);
static int	ofwd_parsedev(struct devdesc **, const char *, const char **);
static bool	ofwd_match(struct devsw *, const char *);

struct devsw ofwdisk = {
	.dv_name = "block",
	.dv_type = DEVT_OFDISK,
	.dv_init = ofwd_init,
	.dv_strategy = ofwd_strategy,
	.dv_open = ofwd_open,
	.dv_close = ofwd_close,
	.dv_ioctl = ofwd_ioctl,
	.dv_print = ofwd_print,
	.dv_cleanup = nullsys,
	.dv_match = ofwd_match,
	.dv_fmtdev = ofwd_fmtdev,
	.dv_parsedev = ofwd_parsedev,
};

/*
 * We're not guaranteed to be able to open a device more than once and there
 * is no OFW standard method to determine whether a device is already opened.
 * Opening a device multiple times simultaneously happens to work with most
 * OFW block device drivers but triggers a trap with at least the driver for
 * the on-board controllers of Sun Fire V100 and Ultra 1.  Upper layers and MI
 * code expect to be able to open a device more than once however.  Given that
 * different partitions of the same device might be opened at the same time as
 * done by ZFS, we can't generally just keep track of the opened devices and
 * reuse the instance handle when asked to open an already opened device.  So
 * the best we can do is to cache the lastly used device path and close and
 * open devices in ofwd_strategy() as needed.
 */
static struct ofw_devdesc *kdp;

static int
ofwd_init(void)
{

	return (0);
}

static int
ofwd_strategy(void *devdata, int flag __unused, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
	struct ofw_devdesc *dp = (struct ofw_devdesc *)devdata;
	daddr_t pos;
	int n;

	if (dp != kdp) {
		if (kdp != NULL) {
#if !defined(__powerpc__)
			OF_close(kdp->d_handle);
#endif
			kdp = NULL;
		}
		if ((dp->d_handle = OF_open(dp->d_path)) == -1)
			return (ENOENT);
		kdp = dp;
	}

	pos = dblk * 512;
	do {
		if (OF_seek(dp->d_handle, pos) < 0)
			return (EIO);
		n = OF_read(dp->d_handle, buf, size);
		if (n < 0 && n != -2)
			return (EIO);
	} while (n == -2);
	*rsize = size;
	return (0);
}

static int
ofwd_open(struct open_file *f, ...)
{
	struct ofw_devdesc *dp;
	va_list vl;

	va_start(vl, f);
	dp = va_arg(vl, struct ofw_devdesc *);
	va_end(vl);

	if (dp != kdp) {
		if (kdp != NULL) {
			OF_close(kdp->d_handle);
			kdp = NULL;
		}
		if ((dp->d_handle = OF_open(dp->d_path)) == -1) {
			printf("%s: Could not open %s\n", __func__,
			    dp->d_path);
			return (ENOENT);
		}
		kdp = dp;
	}
	return (0);
}

static int
ofwd_close(struct open_file *f)
{
	struct ofw_devdesc *dev = f->f_devdata;

	if (dev == kdp) {
#if !defined(__powerpc__)
		OF_close(dev->d_handle);
#endif
		kdp = NULL;
	}
	return (0);
}

static int
ofwd_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct ofw_devdesc *dev = f->f_devdata;
	int block_size;
	unsigned int n;

	switch (cmd) {
	case DIOCGSECTORSIZE:
		block_size = OF_block_size(dev->d_handle);
		*(u_int *)data = block_size;
		break;
	case DIOCGMEDIASIZE:
		block_size = OF_block_size(dev->d_handle);
		n = OF_blocks(dev->d_handle);
		*(uint64_t *)data = ((uint64_t)n * block_size);
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static int
ofwd_print(int verbose __unused)
{
	uintmax_t block_size, n;
	int ret;
	char line[80];

	/*
	 * We don't have a list of devices since we don't parse the whole OFW
	 * tree to find them. Instead, if we have an open device print info
	 * about it. Otherwise say we can't. Makes lsdev nicer.
	 */
	if ((ret = pager_output("block devices:\n")) != 0)
		return (ret);
	if (kdp != NULL) {
		block_size = OF_block_size(kdp->d_handle);
		n = OF_blocks(kdp->d_handle);
		snprintf(line, sizeof(line),
		    "    %s:   OFW block device (%ju X %ju): %ju bytes\n",
		    kdp->d_path, n, block_size, n * block_size);
		if ((ret = pager_output(line)) != 0)
			return (ret);
	} else {
		if ((ret = pager_output("  none are open, so no info\n")) != 0)
			return (ret);
	}

	return (0);
}


static bool
ofwd_match(struct devsw *devsw, const char *devspec)
{
	const char *path;

	return (ofw_path_to_handle(devspec, devsw->dv_name, &path) != -1);
}

static char *
ofwd_fmtdev(struct devdesc *idev)
{
	struct ofw_devdesc *dev = (struct ofw_devdesc *)idev;

	return (dev->d_path);
}

static int
ofwd_parsedev(struct devdesc **dev, const char *devspec, const char **path)
{
	return (ofw_common_parsedev(dev, devspec, path, ofwdisk.dv_name));
}
