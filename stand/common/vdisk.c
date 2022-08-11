/*-
 * Copyright 2019 Toomas Soome <tsoome@me.com>
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

#include <stand.h>
#include <stdarg.h>
#include <machine/_inttypes.h>
#include <bootstrap.h>
#include <sys/disk.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <disk.h>

static int vdisk_init(void);
static int vdisk_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int vdisk_open(struct open_file *, ...);
static int vdisk_close(struct open_file *);
static int vdisk_ioctl(struct open_file *, u_long, void *);
static int vdisk_print(int);

struct devsw vdisk_dev = {
	.dv_name = "vdisk",
	.dv_type = DEVT_DISK,
	.dv_init = vdisk_init,
	.dv_strategy = vdisk_strategy,
	.dv_open = vdisk_open,
	.dv_close = vdisk_close,
	.dv_ioctl = vdisk_ioctl,
	.dv_print = vdisk_print,
	.dv_cleanup = nullsys,
	.dv_fmtdev = disk_fmtdev,
};

typedef STAILQ_HEAD(vdisk_info_list, vdisk_info) vdisk_info_list_t;

typedef struct vdisk_info
{
	STAILQ_ENTRY(vdisk_info)	vdisk_link; /* link in device list */
	char			*vdisk_path;
	int			vdisk_unit;
	int			vdisk_fd;
	uint64_t		vdisk_size;	/* size in bytes */
	uint32_t		vdisk_sectorsz;
	uint32_t		vdisk_open;	/* reference counter */
} vdisk_info_t;

static vdisk_info_list_t vdisk_list;	/* list of mapped vdisks. */

static vdisk_info_t *
vdisk_get_info(struct devdesc *dev)
{
	vdisk_info_t *vd;

	STAILQ_FOREACH(vd, &vdisk_list, vdisk_link) {
		if (vd->vdisk_unit == dev->d_unit)
			return (vd);
	}
	return (vd);
}

COMMAND_SET(map_vdisk, "map-vdisk", "map file as virtual disk", command_mapvd);

static int
command_mapvd(int argc, char *argv[])
{
	vdisk_info_t *vd, *p;
	struct stat sb;

	if (argc != 2) {
		printf("usage: %s filename\n", argv[0]);
		return (CMD_ERROR);
	}

	STAILQ_FOREACH(vd, &vdisk_list, vdisk_link) {
		if (strcmp(vd->vdisk_path, argv[1]) == 0) {
			printf("%s: file %s is already mapped as %s%d\n",
			    argv[0], argv[1], vdisk_dev.dv_name,
			    vd->vdisk_unit);
			return (CMD_ERROR);
		}
	}

	if (stat(argv[1], &sb) < 0) {
		/*
		 * ENOSYS is really ENOENT because we did try to walk
		 * through devsw list to try to open this file.
		 */
		if (errno == ENOSYS)
			errno = ENOENT;

		printf("%s: stat failed: %s\n", argv[0], strerror(errno));
		return (CMD_ERROR);
	}

	/*
	 * Avoid mapping small files.
	 */
	if (sb.st_size < 1024 * 1024) {
		printf("%s: file %s is too small.\n", argv[0], argv[1]);
		return (CMD_ERROR);
	}

	vd = calloc(1, sizeof (*vd));
	if (vd == NULL) {
		printf("%s: out of memory\n", argv[0]);
		return (CMD_ERROR);
	}
	vd->vdisk_path = strdup(argv[1]);
	if (vd->vdisk_path == NULL) {
		free (vd);
		printf("%s: out of memory\n", argv[0]);
		return (CMD_ERROR);
	}
	vd->vdisk_fd = open(vd->vdisk_path, O_RDONLY);
	if (vd->vdisk_fd < 0) {
		printf("%s: open failed: %s\n", argv[0], strerror(errno));
		free(vd->vdisk_path);
		free(vd);
		return (CMD_ERROR);
	}

	vd->vdisk_size = sb.st_size;
	vd->vdisk_sectorsz = DEV_BSIZE;
	STAILQ_FOREACH(p, &vdisk_list, vdisk_link) {
		vdisk_info_t *n;
		if (p->vdisk_unit == vd->vdisk_unit) {
			vd->vdisk_unit++;
			continue;
		}
		n = STAILQ_NEXT(p, vdisk_link);
		if (p->vdisk_unit < vd->vdisk_unit) {
			if (n == NULL) {
				/* p is last elem */
				STAILQ_INSERT_TAIL(&vdisk_list, vd, vdisk_link);
				break;
			}
			if (n->vdisk_unit > vd->vdisk_unit) {
				/* p < vd < n */
				STAILQ_INSERT_AFTER(&vdisk_list, p, vd,
				    vdisk_link);
				break;
			}
			/* else n < vd or n == vd */
			vd->vdisk_unit++;
			continue;
		}
		/* p > vd only if p is the first element */
		STAILQ_INSERT_HEAD(&vdisk_list, vd, vdisk_link);
		break;
	}

	/* if the list was empty or contiguous */
	if (p == NULL)
		STAILQ_INSERT_TAIL(&vdisk_list, vd, vdisk_link);

	printf("%s: file %s is mapped as %s%d\n", argv[0], vd->vdisk_path,
	    vdisk_dev.dv_name, vd->vdisk_unit);
	return (CMD_OK);
}

COMMAND_SET(unmap_vdisk, "unmap-vdisk", "unmap virtual disk", command_unmapvd);

/*
 * unmap-vdisk vdiskX
 */
static int
command_unmapvd(int argc, char *argv[])
{
	size_t len;
	vdisk_info_t *vd;
	long unit;
	char *end;

	if (argc != 2) {
		printf("usage: %s %sN\n", argv[0], vdisk_dev.dv_name);
		return (CMD_ERROR);
	}

	len = strlen(vdisk_dev.dv_name);
	if (strncmp(vdisk_dev.dv_name, argv[1], len) != 0) {
		printf("%s: unknown device %s\n", argv[0], argv[1]);
		return (CMD_ERROR);
	}
	errno = 0;
	unit = strtol(argv[1] + len, &end, 10);
	if (errno != 0 || (*end != '\0' && strcmp(end, ":") != 0)) {
		printf("%s: unknown device %s\n", argv[0], argv[1]);
		return (CMD_ERROR);
	}

	STAILQ_FOREACH(vd, &vdisk_list, vdisk_link) {
		if (vd->vdisk_unit == unit)
			break;
	}

	if (vd == NULL) {
		printf("%s: unknown device %s\n", argv[0], argv[1]);
		return (CMD_ERROR);
	}

	if (vd->vdisk_open != 0) {
		printf("%s: %s is in use, unable to unmap.\n",
		    argv[0], argv[1]);
		return (CMD_ERROR);
	}

	STAILQ_REMOVE(&vdisk_list, vd, vdisk_info, vdisk_link);
	(void) close(vd->vdisk_fd);
	printf("%s (%s) unmapped\n", argv[1], vd->vdisk_path);
	free(vd->vdisk_path);
	free(vd);

	return (CMD_OK);
}

static int
vdisk_init(void)
{
	STAILQ_INIT(&vdisk_list);
	return (0);
}

static int
vdisk_strategy(void *devdata, int rw, daddr_t blk, size_t size,
    char *buf, size_t *rsize)
{
	struct disk_devdesc *dev;
	vdisk_info_t *vd;
	ssize_t rv;

	dev = devdata;
	if (dev == NULL)
		return (EINVAL);
	vd = vdisk_get_info((struct devdesc *)dev);
	if (vd == NULL)
		return (EINVAL);

	if (size == 0 || (size % 512) != 0)
		return (EIO);

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		daddr_t offset;

		offset = dev->d_offset * vd->vdisk_sectorsz;
		offset /= 512;
		blk += offset;
	}
	if (lseek(vd->vdisk_fd, blk << 9, SEEK_SET) == -1)
		return (EIO);

	errno = 0;
	switch (rw & F_MASK) {
	case F_READ:
		rv = read(vd->vdisk_fd, buf, size);
		break;
	case F_WRITE:
		rv = write(vd->vdisk_fd, buf, size);
		break;
	default:
		return (ENOSYS);
	}

	if (errno == 0 && rsize != NULL) {
		*rsize = rv;
	}
	return (errno);
}

static int
vdisk_open(struct open_file *f, ...)
{
	va_list args;
	struct disk_devdesc *dev;
	vdisk_info_t *vd;
	int rc = 0;

	va_start(args, f);
	dev = va_arg(args, struct disk_devdesc *);
	va_end(args);
	if (dev == NULL)
		return (EINVAL);
	vd = vdisk_get_info((struct devdesc *)dev);
	if (vd == NULL)
		return (EINVAL);

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		rc = disk_open(dev, vd->vdisk_size, vd->vdisk_sectorsz);
	}
	if (rc == 0)
		vd->vdisk_open++;
	return (rc);
}

static int
vdisk_close(struct open_file *f)
{
	struct disk_devdesc *dev;
	vdisk_info_t *vd;

	dev = (struct disk_devdesc *)(f->f_devdata);
	if (dev == NULL)
		return (EINVAL);
	vd = vdisk_get_info((struct devdesc *)dev);
	if (vd == NULL)
		return (EINVAL);

	vd->vdisk_open--;
	if (dev->dd.d_dev->dv_type == DEVT_DISK)
		return (disk_close(dev));
	return (0);
}

static int
vdisk_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct disk_devdesc *dev;
	vdisk_info_t *vd;
	int rc;

	dev = (struct disk_devdesc *)(f->f_devdata);
	if (dev == NULL)
		return (EINVAL);
	vd = vdisk_get_info((struct devdesc *)dev);
	if (vd == NULL)
		return (EINVAL);

	if (dev->dd.d_dev->dv_type == DEVT_DISK) {
		rc = disk_ioctl(dev, cmd, data);
		if (rc != ENOTTY)
			return (rc);
	}

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = vd->vdisk_sectorsz;
		break;
	case DIOCGMEDIASIZE:
		*(uint64_t *)data = vd->vdisk_size;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static int
vdisk_print(int verbose)
{
	int ret = 0;
	vdisk_info_t *vd;
	char line[80];

	if (STAILQ_EMPTY(&vdisk_list))
		return (ret);

	printf("%s devices:", vdisk_dev.dv_name);
	if ((ret = pager_output("\n")) != 0)
		return (ret);

	STAILQ_FOREACH(vd, &vdisk_list, vdisk_link) {
		struct disk_devdesc vd_dev;

		if (verbose) {
			printf("  %s", vd->vdisk_path);
			if ((ret = pager_output("\n")) != 0)
				break;
		}
		snprintf(line, sizeof(line),
		    "    %s%d", vdisk_dev.dv_name, vd->vdisk_unit);
		printf("%s:    %" PRIu64 " X %u blocks", line,
		    vd->vdisk_size / vd->vdisk_sectorsz,
		    vd->vdisk_sectorsz);
		if ((ret = pager_output("\n")) != 0)
			break;

		vd_dev.dd.d_dev = &vdisk_dev;
		vd_dev.dd.d_unit = vd->vdisk_unit;
		vd_dev.d_slice = -1;
		vd_dev.d_partition = -1;

		ret = disk_open(&vd_dev, vd->vdisk_size, vd->vdisk_sectorsz);
		if (ret == 0) {
			ret = disk_print(&vd_dev, line, verbose);
			disk_close(&vd_dev);
			if (ret != 0)
				break;
		} else {
			ret = 0;
		}
	}

	return (ret);
}
