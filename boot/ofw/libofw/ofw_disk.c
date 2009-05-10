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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Disk I/O routines using Open Firmware
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <netinet/in.h>

#include <machine/stdarg.h>

#include <stand.h>

#include "bootstrap.h"
#include "libofw.h"

static int	ofwd_init(void);
static int	ofwd_strategy(void *devdata, int flag, daddr_t dblk, 
				size_t size, char *buf, size_t *rsize);
static int	ofwd_open(struct open_file *f, ...);
static int	ofwd_close(struct open_file *f);
static int	ofwd_ioctl(struct open_file *f, u_long cmd, void *data);
static void	ofwd_print(int verbose);

struct devsw ofwdisk = {
	"block",
	DEVT_DISK,
	ofwd_init,
	ofwd_strategy,
	ofwd_open,
	ofwd_close,
	ofwd_ioctl,
	ofwd_print
};

struct opened_dev {
	ihandle_t		handle;
	u_int			count;
	SLIST_ENTRY(opened_dev)	link;
};

SLIST_HEAD(, opened_dev) opened_devs = SLIST_HEAD_INITIALIZER(opened_dev);

static int
ofwd_init(void)
{

	return 0;
}

static int
ofwd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, char *buf,
    size_t *rsize)
{
	struct ofw_devdesc *dp = (struct ofw_devdesc *)devdata;
	daddr_t pos;
	int n;

	pos = dblk * 512;
	do {
		if (OF_seek(dp->d_handle, pos) < 0)
			return EIO;
		n = OF_read(dp->d_handle, buf, size);
		if (n < 0 && n != -2)
			return EIO;
	} while (n == -2);
	*rsize = size;
	return 0;
}

static int
ofwd_open(struct open_file *f, ...)
{
	char path[256];
	struct ofw_devdesc *dp;
	struct opened_dev *odp;
	va_list vl;

	va_start(vl, f);
	dp = va_arg(vl, struct ofw_devdesc *);
	va_end(vl);
	/*
	 * We're not guaranteed to be able to open a device more than once
	 * simultaneously and there is no OFW standard method to determine
	 * whether a device is already opened. Opening a device more than
	 * once happens to work with most OFW block device drivers but
	 * triggers a trap with at least the driver for the on-board SCSI
	 * controller in Sun Ultra 1. Upper layers and MI code expect to
	 * be able to open a device more than once however. As a workaround
	 * keep track of the opened devices and reuse the instance handle
	 * when asked to open an already opened device.
	 */
	SLIST_FOREACH(odp, &opened_devs, link) {
		if (OF_instance_to_path(odp->handle, path, sizeof(path)) == -1)
			continue;
		if (strcmp(path, dp->d_path) == 0) {
			odp->count++;
			dp->d_handle = odp->handle;
			return 0;
		}
	}
	odp = malloc(sizeof(struct opened_dev));
	if (odp == NULL) {
		printf("ofwd_open: malloc failed\n");
		return ENOMEM;
	}
	if ((odp->handle = OF_open(dp->d_path)) == -1) {
		printf("ofwd_open: Could not open %s\n", dp->d_path);
		free(odp);
		return ENOENT;
	}
	odp->count = 1;
	SLIST_INSERT_HEAD(&opened_devs, odp, link);
	dp->d_handle = odp->handle;
	return 0;
}

static int
ofwd_close(struct open_file *f)
{
	struct ofw_devdesc *dev = f->f_devdata;
	struct opened_dev *odp;

	SLIST_FOREACH(odp, &opened_devs, link) {
		if (odp->handle == dev->d_handle) {
			odp->count--;
			if (odp->count == 0) {
				SLIST_REMOVE(&opened_devs, odp, opened_dev,
				    link);
			#if !defined(__powerpc__)
				OF_close(odp->handle);
			#endif
				free(odp);
			}
			break;
		}
	}
	return 0;
}

static int
ofwd_ioctl(struct open_file *f, u_long cmd, void *data)
{

	return (EINVAL);
}

static void
ofwd_print(int verbose)
{

}
