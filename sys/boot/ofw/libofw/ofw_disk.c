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
static int probed;

#define	OFDP_FOUND	0
#define	OFDP_NOTFOUND	1
#define	OFDP_TERMINATE	2

#define	MAXDEV_IDE	4
#define	MAXDEV_DEFAULT	16	/* SCSI etc. */

void
ofwd_enter_dev(const char *devpath)
{
	char *p;
	int n;

	if (ofwd_getunit(devpath) != -1)
		return;
	if ((p = strrchr(devpath, ',')) != NULL)
		n = p - devpath;
	else
		n = strlen(devpath);
	ofwdinfo[nofwdinfo].ofwd_unit = nofwdinfo;
	strncpy(ofwdinfo[nofwdinfo].ofwd_path, devpath, n);
	ofwdinfo[nofwdinfo].ofwd_path[n] = '\0';
	printf("disk%d is %s\n", nofwdinfo, ofwdinfo[nofwdinfo].ofwd_path);
	nofwdinfo++;
}

static int
ofwd_probe_dev(char *devpath)
{
	ihandle_t instance;
	int rv;

	/* Is the device already in the list? */
	if (ofwd_getunit(devpath) != -1)
		return OFDP_FOUND;
	instance = OF_open(devpath);
	if (instance != -1) {
		ofwd_enter_dev(devpath);
		OF_close(instance);
	} else
		return OFDP_NOTFOUND;
	if (nofwdinfo > MAXDEV) {
		printf("Hit MAXDEV probing disks.\n");
		return OFDP_TERMINATE;
	}
	return OFDP_FOUND;
}

static int
ofwd_probe_devs(void)
{
	int ret;
	char devpath[255];
#ifdef __sparc64__
	int i, n;
	char cdevpath[255];
#endif

	probed = 1;
	ofw_devsearch_init();
	while ((ret = ofw_devsearch("block", devpath)) != 0) {
		devpath[sizeof devpath - 1] = 0;
		if (ret == -1)
			return 1;
#ifdef DEBUG
		printf("devpath=\"%s\" ret=%d\n", devpath, ret);
#endif

		if (strstr(devpath, "cdrom") != 0)
			continue;

#ifdef __sparc64__
		/*
		 * sparc64 machines usually only have a single disk node as
		 * child of the controller (in the ATA case, there may exist
		 * an additional cdrom node, which we ignore above, since
		 * booting from it is special, and it can also be used as a
		 * disk node).
		 * Devices are accessed by using disk@unit; when no unit
		 * number is given, 0 is assumed.
		 * There is no way we can enumerate the existing disks except
		 * trying to open them, which unfortunately creates some deleays
		 * and spurioius warnings printed by the prom, which we can't
		 * do much about. The search may not stop on the first
		 * unsuccessful attempt, because that would cause disks that
		 * follow one with an invalid label (like CD-ROMS) would not
		 * be detected this way.
		 * Try to at least be a bit smart and only probe 4 devices in
		 * the IDE case.
		 */
		if (strstr(devpath, "/ide@") != NULL)
			n = MAXDEV_IDE;
		else
			n = MAXDEV_DEFAULT;
		for (i = 0; i < n; i++) {
			sprintf(cdevpath, "%s@%d", devpath, i);
			if (ofwd_probe_dev(cdevpath) == OFDP_TERMINATE)
				return 1;
		}
#else
		if (ofwd_probe_dev(devpath) == OFDP_TERMINATE)
			return 1;
#endif
	}

	return 0;
}

static int
ofwd_init(void)
{
	/* Short-circuit the device probing, since it takes too long. */
	return 0;
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
	char buf[256];
	int i, j;

	va_start(vl, f);
	dp = va_arg(vl, struct ofw_devdesc *);
	va_end(vl);

	/*
	 * The unit number is really an index into our device array.
	 * If it is not in the list, we may need to probe now.
	 */
	if (!probed && dp->d_kind.ofwdisk.unit >= nofwdinfo)
		ofwd_probe_devs();
	if (dp->d_kind.ofwdisk.unit >= nofwdinfo)
		return 1;
	devpath = ofwdinfo[dp->d_kind.ofwdisk.unit].ofwd_path;
	sprintf(buf, "%s,%d:%c", devpath, dp->d_kind.ofwdisk.slice,
	    'a' + dp->d_kind.ofwdisk.partition);
	if ((diskh = OF_open(buf)) == -1) {
		printf("ofwd_open: Could not open %s\n", buf);
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

	if (!probed)
		ofwd_probe_devs();
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
	char *p;
	int i, n;

	if ((p = strrchr(path, ',')) != NULL)
		n = p - path;
	else
		n = strlen(path);
	for (i = 0; i < nofwdinfo; i++) {
		if (strncmp(path, ofwdinfo[i].ofwd_path, n) == 0)
			return i;
	}

	return -1;
}
