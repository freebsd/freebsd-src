/*-
 * Copyright (C) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright 2022 Netflix, Inc
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
#include <sys/disk.h>
#include <stdarg.h>
#include <paths.h>
#include "host_syscall.h"
#include "kboot.h"
#include "bootstrap.h"
#ifdef LOADER_ZFS_SUPPORT
#include "libzfs.h"
#include <sys/zfs_bootenv.h>
#endif

static int hostdisk_init(void);
static int hostdisk_strategy(void *devdata, int flag, daddr_t dblk,
    size_t size, char *buf, size_t *rsize);
static int hostdisk_open(struct open_file *f, ...);
static int hostdisk_close(struct open_file *f);
static int hostdisk_ioctl(struct open_file *f, u_long cmd, void *data);
static int hostdisk_print(int verbose);
static char *hostdisk_fmtdev(struct devdesc *vdev);
static bool hostdisk_match(struct devsw *devsw, const char *devspec);
static int hostdisk_parsedev(struct devdesc **idev, const char *devspec, const char **path);

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
	.dv_match = hostdisk_match,
	.dv_parsedev = hostdisk_parsedev,
};

/*
 * We need to walk through the /sys/block directories looking for
 * block devices that we can use.
 */
#define SYSBLK "/sys/block"

#define HOSTDISK_MIN_SIZE (16ul << 20)	/* 16MB */

typedef STAILQ_HEAD(, hdinfo) hdinfo_list_t;
typedef struct hdinfo {
	STAILQ_ENTRY(hdinfo)	hd_link;	/* link in device list */
	hdinfo_list_t	hd_children;
	struct hdinfo	*hd_parent;
	const char	*hd_dev;
	uint64_t	hd_size;		/* In bytes */
	uint64_t	hd_sectors;
	uint64_t	hd_sectorsize;
	int		hd_flags;
#define HDF_HAS_ZPOOL	1			/* We found a zpool here and uuid valid */
	uint64_t	hd_zfs_uuid;
} hdinfo_t;

#define dev2hd(d) ((hdinfo_t *)d->d_opendata)
#define hd_name(hd) ((hd->hd_dev + 5))

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
hostdisk_add_part(hdinfo_t *hd, const char *drv, uint64_t secs)
{
	hdinfo_t *md;
	char *dev;

	printf("hd %s adding %s %ju\n", hd->hd_dev, drv, (uintmax_t)secs);
	if ((md = calloc(1, sizeof(*md))) == NULL)
		return;
	if (asprintf(&dev, "/dev/%s", drv) == -1) {
		printf("hostdisk: no memory\n");
		free(md);
		return;
	}
	md->hd_dev = dev;
	md->hd_sectors = secs;
	md->hd_sectorsize = hd->hd_sectorsize;
	md->hd_size = md->hd_sectors * md->hd_sectorsize;
	md->hd_parent = hd;
	STAILQ_INSERT_TAIL(&hd->hd_children, md, hd_link);
}

static bool
hostdisk_one_part(struct host_dirent64 *dent, void *argp)
{
	hdinfo_t *hd = argp;
	char szfn[1024];
	uint64_t sz;

	/* Need to skip /dev/ at start of hd_name */
	if (strncmp(dent->d_name, hd_name(hd), strlen(hd_name(hd))) != 0)
		return (true);
	/* Find out how big this is -- no size not a disk */
	snprintf(szfn, sizeof(szfn), "%s/%s/%s/size", SYSBLK,
	    hd_name(hd), dent->d_name);
	if (!file2u64(szfn, &sz))
		return true;
	hostdisk_add_part(hd, dent->d_name, sz);
	return true;
}

static void
hostdisk_add_parts(hdinfo_t *hd)
{
	char fn[1024];

	snprintf(fn, sizeof(fn), "%s/%s", SYSBLK, hd_name(hd));
	foreach_file(fn, hostdisk_one_part, hd, 0);
}

static void
hostdisk_add_drive(const char *drv, uint64_t secs)
{
	hdinfo_t *hd = NULL;
	char *dev = NULL;
	char fn[1024];

	if ((hd = calloc(1, sizeof(*hd))) == NULL)
		return;
	if (asprintf(&dev, "/dev/%s", drv) == -1) {
		printf("hostdisk: no memory\n");
		free(hd);
		return;
	}
	hd->hd_dev = dev;
	hd->hd_sectors = secs;
	snprintf(fn, sizeof(fn), "%s/%s/queue/hw_sector_size",
	    SYSBLK, drv);
	if (!file2u64(fn, &hd->hd_sectorsize))
		goto err;
	hd->hd_size = hd->hd_sectors * hd->hd_sectorsize;
	if (hd->hd_size < HOSTDISK_MIN_SIZE)
		goto err;
	hd->hd_flags = 0;
	STAILQ_INIT(&hd->hd_children);
	printf("/dev/%s: %ju %ju %ju\n",
	    drv, hd->hd_size, hd->hd_sectors, hd->hd_sectorsize);
	STAILQ_INSERT_TAIL(&hdinfo, hd, hd_link);
	hostdisk_add_parts(hd);
	return;
err:
	free(dev);
	free(hd);
	return;
}

/* Find a disk / partition by its filename */

static hdinfo_t *
hostdisk_find(const char *fn)
{
	hdinfo_t *hd, *md;

	STAILQ_FOREACH(hd, &hdinfo, hd_link) {
		if (strcmp(hd->hd_dev, fn) == 0)
			return (hd);
		STAILQ_FOREACH(md, &hd->hd_children, hd_link) {
			if (strcmp(md->hd_dev, fn) == 0)
				return (md);
		}
	}
	return (NULL);
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

static bool
hostdisk_fake_one_disk(struct host_dirent64 *dent, void *argp)
{
	char *override_dir = argp;
	char *fn = NULL;
	hdinfo_t *hd = NULL;
	struct host_kstat sb;

	/*
	 * We only do regular files. Each one is treated as a disk image
	 * accessible via /dev/${dent->d_name}.
	 */
	if (dent->d_type != HOST_DT_REG && dent->d_type != HOST_DT_LNK)
		return (true);
	if (asprintf(&fn, "%s/%s", override_dir, dent->d_name) == -1)
		return (true);
	if (host_stat(fn, &sb) != 0)
		goto err;
	if (!HOST_S_ISREG(sb.st_mode))
		return (true);
	if (sb.st_size == 0)
		goto err;
	if ((hd = calloc(1, sizeof(*hd))) == NULL)
		goto err;
	hd->hd_dev = fn;
	hd->hd_size = sb.st_size;
	hd->hd_sectorsize = 512;	/* XXX configurable? */
	hd->hd_sectors = hd->hd_size / hd->hd_sectorsize;
	if (hd->hd_size < HOSTDISK_MIN_SIZE)
		goto err;
	hd->hd_flags = 0;
	STAILQ_INIT(&hd->hd_children);
	printf("%s: %ju %ju %ju\n",
	    hd->hd_dev, hd->hd_size, hd->hd_sectors, hd->hd_sectorsize);
	STAILQ_INSERT_TAIL(&hdinfo, hd, hd_link);
	/* XXX no partiions? -- is that OK? */
	return (true);
err:
	free(hd);
	free(fn);
	return (true);
}

static void
hostdisk_find_block_devices(void)
{
	char *override;

	override=getenv("hostdisk_override");
	if (override != NULL)
		foreach_file(override, hostdisk_fake_one_disk, override, 0);
	else
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
	const char *fn;
	va_list vl;

	va_start(vl, f);
	desc = va_arg(vl, struct devdesc *);
	va_end(vl);

	fn = dev2hd(desc)->hd_dev;
	desc->d_unit = host_open(fn, O_RDONLY, 0);
	if (desc->d_unit <= 0) {
		printf("hostdisk_open: couldn't open %s: %d\n", fn, errno);
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
	struct devdesc *desc = f->f_devdata;
	hdinfo_t *hd = dev2hd(desc);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = hd->hd_sectorsize;
		break;
	case DIOCGMEDIASIZE:
		*(uint64_t *)data = hd->hd_size;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
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
		    "   %s: %ju X %ju: %ju bytes\n",
		    hd->hd_dev,
		    (uintmax_t)hd->hd_sectors,
		    (uintmax_t)hd->hd_sectorsize,
		    (uintmax_t)hd->hd_size);
		if ((ret = pager_output(line)) != 0)
			break;
		STAILQ_FOREACH(md, &hd->hd_children, hd_link) {
			snprintf(line, sizeof(line),
			    "     %s: %ju X %ju: %ju bytes\n",
			    md->hd_dev,
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

	return ((char *)hd_name(dev2hd(vdev)));
}

static bool
hostdisk_match(struct devsw *devsw, const char *devspec)
{
	hdinfo_t *hd;
	const char *colon;
	char *cp;

	colon = strchr(devspec, ':');
	if (colon == NULL)
		return false;
	cp = strdup(devspec);
	cp[colon - devspec] = '\0';
	hd = hostdisk_find(cp);
	free(cp);
	return (hd != NULL);
}

static int
hostdisk_parsedev(struct devdesc **idev, const char *devspec, const char **path)
{
	const char *cp;
	struct devdesc *dev;
	hdinfo_t *hd;
	int len;
	char *fn;

	/* Must have a : in it */
	cp = strchr(devspec, ':');
	if (cp == NULL)
		return (EINVAL);
	/* XXX Stat the /dev or defer error handling to open(2) call? */
	if (path != NULL)
		*path = cp + 1;
	len = cp - devspec;
	fn = strdup(devspec);
	fn[len] = '\0';
	hd = hostdisk_find(fn);
	if (hd == NULL) {
		printf("Can't find hdinfo for %s\n", fn);
		free(fn);
		return (EINVAL);
	}
	free(fn);
	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return (ENOMEM);
	dev->d_unit = 0;
	dev->d_dev = &hostdisk;
	dev->d_opendata = hd;
	*idev = dev;
	return (0);
}

#ifdef LOADER_ZFS_SUPPORT
static bool
hostdisk_zfs_check_one(hdinfo_t *hd)
{
	char *fn;
	bool found = false;
	uint64_t pool_uuid;

	if (asprintf(&fn, "%s:", hd->hd_dev) == -1)
		return (false);
	pool_uuid = 0;
	zfs_probe_dev(fn, &pool_uuid, false);
	if (pool_uuid != 0) {
		found = true;
		hd->hd_flags |= HDF_HAS_ZPOOL;
		hd->hd_zfs_uuid = pool_uuid;
	}
	free(fn);

	return (found);
}

void
hostdisk_zfs_probe(void)
{
	hdinfo_t *hd, *md;

	STAILQ_FOREACH(hd, &hdinfo, hd_link) {
		if (hostdisk_zfs_check_one(hd))
			continue;
		STAILQ_FOREACH(md, &hd->hd_children, hd_link) {
			hostdisk_zfs_check_one(md);
		}
	}
}

/* XXX refactor */
static bool
sanity_check_currdev(void)
{
	struct stat st;

	return (stat(PATH_DEFAULTS_LOADER_CONF, &st) == 0 ||
#ifdef PATH_BOOTABLE_TOKEN
	    stat(PATH_BOOTABLE_TOKEN, &st) == 0 || /* non-standard layout */
#endif
	    stat(PATH_KERNEL, &st) == 0);
}

/* This likely shoud move to libsa/zfs/zfs.c and be used by at least EFI booting */
static bool
probe_zfs_currdev(uint64_t pool_guid, uint64_t root_guid, bool setcurrdev)
{
	char *devname;
	struct zfs_devdesc currdev;
	char *buf = NULL;
	bool bootable;

	currdev.dd.d_dev = &zfs_dev;
	currdev.dd.d_unit = 0;
	currdev.pool_guid = pool_guid;
	currdev.root_guid = root_guid;
	devname = devformat(&currdev.dd);
	if (setcurrdev)
		set_currdev(devname);

	bootable = sanity_check_currdev();
	if (bootable) {
		buf = malloc(VDEV_PAD_SIZE);
		if (buf != NULL) {
			if (zfs_get_bootonce(&currdev, OS_BOOTONCE, buf,
			    VDEV_PAD_SIZE) == 0) {
				printf("zfs bootonce: %s\n", buf);
				if (setcurrdev)
					set_currdev(buf);
				setenv("zfs-bootonce", buf, 1);
			}
			free(buf);
			(void)zfs_attach_nvstore(&currdev);
		}
		init_zfs_boot_options(devname);
	}
	return (bootable);
}

static bool
hostdisk_zfs_try_default(hdinfo_t *hd)
{
	return (probe_zfs_currdev(hd->hd_zfs_uuid, 0, true));
}

bool
hostdisk_zfs_find_default(void)
{
	hdinfo_t *hd, *md;

	STAILQ_FOREACH(hd, &hdinfo, hd_link) {
		if (hd->hd_flags & HDF_HAS_ZPOOL) {
			if (hostdisk_zfs_try_default(hd))
				return (true);
			continue;
		}
		STAILQ_FOREACH(md, &hd->hd_children, hd_link) {
			if (md->hd_flags & HDF_HAS_ZPOOL) {
				if (hostdisk_zfs_try_default(md))
					return (true);
			}
		}
	}
	return (false);
}

#endif
