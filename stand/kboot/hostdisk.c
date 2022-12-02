/*-
 * Copyright (C) 2014 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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

#include <sys/types.h>
#include <stdarg.h>
#include "host_syscall.h"
#include "kboot.h"

static int hostdisk_init(void);
static int hostdisk_strategy(void *devdata, int flag, daddr_t dblk,
    size_t size, char *buf, size_t *rsize);
static int hostdisk_open(struct open_file *f, ...);
static int hostdisk_close(struct open_file *f);
static int hostdisk_ioctl(struct open_file *f, u_long cmd, void *data);
static int hostdisk_print(int verbose);
static char *hostdisk_fmtdev(struct devdesc *vdev);

struct devsw hostdisk = {
	.dv_name = "/dev",
	.dv_type = DEVT_HOSTDISK,
	.dv_init = hostdisk_init,
	.dv_strategy = hostdisk_strategy,
	.dv_open = hostdisk_open,
	.dv_close = hostdisk_close,
	.dv_ioctl = hostdisk_ioctl,
	.dv_print = hostdisk_print,
	.dv_cleanup = nullsys,
	.dv_fmtdev = hostdisk_fmtdev,
};

/*
 * We need to walk through the /sys/block directories looking for
 * block devices that we can use.
 */
#define SYSBLK "/sys/block"

typedef STAILQ_HEAD(, hdinfo) hdinfo_list_t;
typedef struct hdinfo {
	STAILQ_ENTRY(hdinfo)	hd_link;	/* link in device list */
	hdinfo_list_t	hd_children;
	struct hdinfo	*hd_parent;
	const char	*hd_name;
	uint64_t	hd_size;
	uint64_t	hd_sectors;
	uint64_t	hd_sectorsize;
	int		hd_flags;
} hdinfo_t;

static hdinfo_list_t hdinfo = STAILQ_HEAD_INITIALIZER(hdinfo);

typedef bool fef_cb_t(struct host_dirent64 *, void *);
#define FEF_RECURSIVE 1

static bool
foreach_file(const char *dir, fef_cb_t cb, void *argp, u_int flags)
{
	char dents[2048];
	int fd, dentsize;
	struct host_dirent64 *dent;

	fd = host_open(dir, O_RDONLY, 0);
	if (fd < 0) {
		printf("Can't open %s\n", dir);/* XXX */
		return (false);
	}
	while (1) {
		dentsize = host_getdents64(fd, dents, sizeof(dents));
		if (dentsize <= 0)
			break;
		for (dent = (struct host_dirent64 *)dents;
		     (char *)dent < dents + dentsize;
		     dent = (struct host_dirent64 *)((void *)dent + dent->d_reclen)) {
			if (!cb(dent, argp))
				break;
		}
	}
	host_close(fd);
	return (true);
}

static void
hostdisk_add_part(struct hdinfo *hd, const char *drv, uint64_t secs)
{
	struct hdinfo *md;

	printf("hd %s adding %s %ju\n", hd->hd_name, drv, (uintmax_t)secs);
	if ((md = calloc(1, sizeof(*md))) == NULL)
		return;
	md->hd_name = strdup(drv);
	md->hd_sectors = secs;
	md->hd_sectorsize = hd->hd_sectorsize;
	md->hd_size = md->hd_sectors * md->hd_sectorsize;
	md->hd_parent = hd;
	STAILQ_INSERT_TAIL(&hd->hd_children, md, hd_link);
}

static bool
hostdisk_one_part(struct host_dirent64 *dent, void *argp)
{
	struct hdinfo *hd = argp;
	char szfn[1024];
	uint64_t sz;

	if (strncmp(dent->d_name, hd->hd_name, strlen(hd->hd_name)) != 0)
		return (true);
	/* Find out how big this is -- no size not a disk */
	snprintf(szfn, sizeof(szfn), "%s/%s/%s/size", SYSBLK,
	    hd->hd_name, dent->d_name);
	if (!file2u64(szfn, &sz))
		return true;
	hostdisk_add_part(hd, dent->d_name, sz);
	return true;
}

static void
hostdisk_add_parts(struct hdinfo *hd)
{
	char fn[1024];

	snprintf(fn, sizeof(fn), "%s/%s", SYSBLK, hd->hd_name);
	foreach_file(fn, hostdisk_one_part, hd, 0);
}

static void
hostdisk_add_drive(const char *drv, uint64_t secs)
{
	struct hdinfo *hd;
	char fn[1024];

	if ((hd = calloc(1, sizeof(*hd))) == NULL)
		return;
	hd->hd_name = strdup(drv);
	hd->hd_sectors = secs;
	snprintf(fn, sizeof(fn), "%s/%s/queue/hw_sector_size",
	    SYSBLK, drv);
	if (!file2u64(fn, &hd->hd_sectorsize))
		goto err;
	hd->hd_size = hd->hd_sectors * hd->hd_sectorsize;
	hd->hd_flags = 0;
	STAILQ_INIT(&hd->hd_children);
	printf("/dev/%s: %ju %ju %ju\n",
	    drv, hd->hd_size, hd->hd_sectors, hd->hd_sectorsize);
	STAILQ_INSERT_TAIL(&hdinfo, hd, hd_link);
	hostdisk_add_parts(hd);
	return;
err:
	free(hd);
	return;
}

static bool
hostdisk_one_disk(struct host_dirent64 *dent, void *argp __unused)
{
	char szfn[1024];
	uint64_t sz;

	/*
	 * Skip . and ..
	 */
	if (strcmp(dent->d_name, ".") == 0 ||
	    strcmp(dent->d_name, "..") == 0)
		return (true);

	/* Find out how big this is -- no size not a disk */
	snprintf(szfn, sizeof(szfn), "%s/%s/size", SYSBLK,
	    dent->d_name);
	if (!file2u64(szfn, &sz))
		return (true);
	hostdisk_add_drive(dent->d_name, sz);
	return (true);
}

static void
hostdisk_find_block_devices(void)
{
	/*
	 * Start here XXX open SYSBLK, walk through all directories, keep the
	 * ones that return a size and a 'block' device when we 'stat' it. Try
	 * to avoid partitions and only do raw devices.
	 */
	foreach_file(SYSBLK, hostdisk_one_disk, NULL, 0);
}

static int
hostdisk_init(void)
{
	hostdisk_find_block_devices();

	return (0);
}

static int
hostdisk_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
	struct devdesc *desc = devdata;
	daddr_t pos;
	int n;
	int64_t off;
	uint64_t res;
	uint32_t posl, posh;

	pos = dblk * 512;

	posl = pos & 0xffffffffu;
	posh = (pos >> 32) & 0xffffffffu;
	if ((off = host_llseek(desc->d_unit, posh, posl, &res, 0)) < 0) {
		printf("Seek error on fd %d to %ju (dblk %ju) returns %jd\n",
		    desc->d_unit, (uintmax_t)pos, (uintmax_t)dblk, (intmax_t)off);
		return (EIO);
	}
	n = host_read(desc->d_unit, buf, size);

	if (n < 0)
		return (EIO);

	*rsize = n;
	return (0);
}

static int
hostdisk_open(struct open_file *f, ...)
{
	struct devdesc *desc;
	va_list vl;

	va_start(vl, f);
	desc = va_arg(vl, struct devdesc *);
	va_end(vl);

	desc->d_unit = host_open(desc->d_opendata, O_RDONLY, 0);
	if (desc->d_unit <= 0) {
		printf("hostdisk_open: couldn't open %s: %d\n",
		    (char *)desc->d_opendata, desc->d_unit);
		return (ENOENT);
	}

	return (0);
}

static int
hostdisk_close(struct open_file *f)
{
	struct devdesc *desc = f->f_devdata;

	host_close(desc->d_unit);
	return (0);
}

static int
hostdisk_ioctl(struct open_file *f, u_long cmd, void *data)
{

	return (EINVAL);
}

static int
hostdisk_print(int verbose)
{
	char line[80];
	hdinfo_t *hd, *md;
	int ret = 0;

	printf("%s devices:", hostdisk.dv_name);
	if (pager_output("\n") != 0)
		return (1);

	STAILQ_FOREACH(hd, &hdinfo, hd_link) {
		snprintf(line, sizeof(line),
		    "   /dev/%s: %ju X %ju: %ju bytes\n",
		    hd->hd_name,
		    (uintmax_t)hd->hd_sectors,
		    (uintmax_t)hd->hd_sectorsize,
		    (uintmax_t)hd->hd_size);
		if ((ret = pager_output(line)) != 0)
			break;
		STAILQ_FOREACH(md, &hd->hd_children, hd_link) {
			snprintf(line, sizeof(line),
			    "     /dev/%s: %ju X %ju: %ju bytes\n",
			    md->hd_name,
			    (uintmax_t)md->hd_sectors,
			    (uintmax_t)md->hd_sectorsize,
			    (uintmax_t)md->hd_size);
			if ((ret = pager_output(line)) != 0)
				goto done;
		}
	}

done:
	return (ret);
}

static char *
hostdisk_fmtdev(struct devdesc *vdev)
{
	return (vdev->d_opendata);
}
