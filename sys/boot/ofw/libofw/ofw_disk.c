/*
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
 *
 * $FreeBSD$
 */

/*
 * Disk I/O routines using Open Firmware
 */

#include <sys/param.h>
#include <sys/disklabel.h>

#include <netinet/in.h>

#include <machine/stdarg.h>

#include <stand.h>

#include "bootstrap.h"
#include "libofw.h"

#define	DISKSECSZ	512

static int	ofwd_init(void);
static int	ofwd_strategy(void *devdata, int flag, daddr_t dblk, 
				size_t size, char *buf, size_t *rsize);
static int	ofwd_open(struct open_file *f, ...);
static int	ofwd_close(struct open_file *f);
static void	ofwd_print(int verbose);
static char *	ofwd_getdevpath(int unit);
int readdisklabel(struct ofw_devdesc *);

struct devsw ofwdisk = {
	"disk",
	DEVT_DISK,
	ofwd_init,
	ofwd_strategy,
	ofwd_open,
	ofwd_close,
	noioctl,
	ofwd_print
};

static struct ofwdinfo {
	int	ofwd_unit;
	char	ofwd_path[255];
} ofwdinfo[MAXDEV];
static int nofwdinfo = 0;

static int
ofwd_init(void)
{
	int ret;
	char devpath[255];
	ihandle_t instance;

	ofw_devsearch_init();
	while ((ret = ofw_devsearch("block", devpath)) != 0) {
		devpath[sizeof devpath - 1] = 0;
		if (ret == -1)
			return (1);
#ifdef DEBUG
		printf("devpath=\"%s\" ret=%d\n", devpath, ret);
#endif

		if (strstr(devpath, "cdrom") != 0)
			continue;

		instance = OF_open(devpath);
		if (instance != -1) {
			ofwdinfo[nofwdinfo].ofwd_unit = nofwdinfo;
			strncpy(ofwdinfo[nofwdinfo].ofwd_path, devpath, 255);
			printf("disk%d is %s\n", nofwdinfo, ofwdinfo[nofwdinfo].ofwd_path);
			nofwdinfo++;
			OF_close(instance);
		}

		if (nofwdinfo > MAXDEV) {
			printf("Hit MAXDEV probing disks.\n");
			return (1);
		}
	}

	return (0);
}

static int
ofwd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, char *buf,
    size_t *rsize)
{
	struct ofw_devdesc *dp = (struct ofw_devdesc *)devdata;
	unsigned long pos;
	int n;
	int i, j;

	pos = (dp->d_kind.ofwdisk.partoff + dblk) * dp->d_kind.ofwdisk.bsize;

	do {
		if (OF_seek(dp->d_kind.ofwdisk.handle, pos) < 0) {
			return EIO;
		}
		n = OF_read(dp->d_kind.ofwdisk.handle, buf, size);
		if (n < 0 && n != -2) {
			return EIO;
		}
	} while (n == -2);

	*rsize = size;
	return 0;
}

static int
ofwd_open(struct open_file *f, ...)
{
	va_list vl;
	struct ofw_devdesc *dp;
	char *devpath;
	phandle_t diskh;
	char buf[8192];
	int i, j;

	va_start(vl, f);
	dp = va_arg(vl, struct ofw_devdesc *);
	va_end(vl);

	devpath = ofwd_getdevpath(dp->d_kind.ofwdisk.unit);
	if ((diskh = OF_open(devpath)) == -1) {
		printf("ofwd_open: Could not open %s\n", devpath);
		return 1;
	}
	dp->d_kind.ofwdisk.bsize = DISKSECSZ;
	dp->d_kind.ofwdisk.handle = diskh;
	readdisklabel(dp);
	
	return 0;
}

int
readdisklabel(struct ofw_devdesc *dp)
{
	char buf[DISKSECSZ];
	struct disklabel *lp;
	size_t size;
	int i;

	dp->d_kind.ofwdisk.partoff = 0;
	dp->d_dev->dv_strategy(dp, 0, LABELSECTOR, sizeof(buf), buf, &size);
	i = dp->d_kind.ofwdisk.partition;
	if (i >= MAXPARTITIONS)
		return 1;

	lp = (struct disklabel *)(buf + LABELOFFSET);
	dp->d_kind.ofwdisk.partoff = lp->d_partitions[i].p_offset;
	return 0;
}

static int
ofwd_close(struct open_file *f)
{
	struct ofw_devdesc *dev = f->f_devdata;
	OF_close(dev->d_kind.ofwdisk.handle);

	return 0;
}

static void
ofwd_print(int verbose)
{
	int	i;
	char	line[80];

	for (i = 0; i < nofwdinfo; i++) {
		sprintf(line, "    disk%d:   %s", i, ofwdinfo[i].ofwd_path);
		pager_output(line);
		pager_output("\n");
	}
	return;
}

int
ofwd_getunit(const char *path)
{
	int i;

	for (i = 0; i < nofwdinfo; i++) {
		if (strcmp(path, ofwdinfo[i].ofwd_path) == 0)
			return i;
	}

	return -1;
}

static char *
ofwd_getdevpath(int unit)
{
	int i;

	for (i = 0; i < nofwdinfo; i++) {
		if (ofwdinfo[i].ofwd_unit == unit)
			return ofwdinfo[i].ofwd_path;
	}
	return 0;
}
