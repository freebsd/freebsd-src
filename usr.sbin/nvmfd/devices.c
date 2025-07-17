/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/disk.h>
#include <sys/gsb_crc32.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/ieee_oui.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libnvmf.h>
#include <libutil.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#define	RAMDISK_PREFIX	"ramdisk:"

struct backing_device {
	enum { RAMDISK, FILE, CDEV } type;
	union {
		int	fd;	/* FILE, CDEV */
		void	*mem;	/* RAMDISK */
	};
	u_int	sector_size;
	uint64_t nlbas;
	uint64_t eui64;
};

static struct backing_device *devices;
static u_int ndevices;

static uint64_t
generate_eui64(uint32_t low)
{
	return (OUI_FREEBSD_NVME_LOW << 16 | low);
}

static uint32_t
crc32(const void *buf, size_t len)
{
	return (calculate_crc32c(0xffffffff, buf, len) ^ 0xffffffff);
}

static void
init_ramdisk(const char *config, struct backing_device *dev)
{
	static uint32_t ramdisk_idx = 1;
	uint64_t num;

	dev->type = RAMDISK;
	dev->sector_size = 512;
	if (expand_number(config, &num))
		errx(1, "Invalid ramdisk specification: %s", config);
	if ((num % dev->sector_size) != 0)
		errx(1, "Invalid ramdisk size %ju", (uintmax_t)num);
	dev->mem = calloc(num, 1);
	dev->nlbas = num / dev->sector_size;
	dev->eui64 = generate_eui64('M' << 24 | ramdisk_idx++);
}

static void
init_filedevice(const char *config, int fd, struct stat *sb,
    struct backing_device *dev)
{
	dev->type = FILE;
	dev->fd = fd;
	dev->sector_size = 512;
	if ((sb->st_size % dev->sector_size) != 0)
		errx(1, "File size is not a multiple of 512: %s", config);
	dev->nlbas = sb->st_size / dev->sector_size;
	dev->eui64 = generate_eui64('F' << 24 |
	    (crc32(config, strlen(config)) & 0xffffff));
}

static void
init_chardevice(const char *config, int fd, struct backing_device *dev)
{
	off_t len;

	dev->type = CDEV;
	dev->fd = fd;
	if (ioctl(fd, DIOCGSECTORSIZE, &dev->sector_size) != 0)
		err(1, "Failed to fetch sector size for %s", config);
	if (ioctl(fd, DIOCGMEDIASIZE, &len) != 0)
		err(1, "Failed to fetch sector size for %s", config);
	dev->nlbas = len / dev->sector_size;
	dev->eui64 = generate_eui64('C' << 24 |
	    (crc32(config, strlen(config)) & 0xffffff));
}

static void
init_device(const char *config, struct backing_device *dev)
{
	struct stat sb;
	int fd;

	/* Check for a RAM disk. */
	if (strncmp(RAMDISK_PREFIX, config, strlen(RAMDISK_PREFIX)) == 0) {
		init_ramdisk(config + strlen(RAMDISK_PREFIX), dev);
		return;
	}

	fd = open(config, O_RDWR);
	if (fd == -1)
		err(1, "Failed to open %s", config);
	if (fstat(fd, &sb) == -1)
		err(1, "fstat");
	switch (sb.st_mode & S_IFMT) {
	case S_IFCHR:
		init_chardevice(config, fd, dev);
		break;
	case S_IFREG:
		init_filedevice(config, fd, &sb, dev);
		break;
	default:
		errx(1, "Invalid file type for %s", config);
	}
}

void
register_devices(int ac, char **av)
{
	ndevices = ac;
	devices = calloc(ndevices, sizeof(*devices));

	for (int i = 0; i < ac; i++)
		init_device(av[i], &devices[i]);
}

u_int
device_count(void)
{
	return (ndevices);
}

static struct backing_device *
lookup_device(uint32_t nsid)
{
	if (nsid == 0 || nsid > ndevices)
		return (NULL);
	return (&devices[nsid - 1]);
}

void
device_active_nslist(uint32_t nsid, struct nvme_ns_list *nslist)
{
	u_int count;

	memset(nslist, 0, sizeof(*nslist));
	count = 0;
	nsid++;
	while (nsid <= ndevices) {
		nslist->ns[count] = htole32(nsid);
		count++;
		if (count == nitems(nslist->ns))
			break;
		nsid++;
	}
}

bool
device_identification_descriptor(uint32_t nsid, void *buf)
{
	struct backing_device *dev;
	char *p;

	dev = lookup_device(nsid);
	if (dev == NULL)
		return (false);

	memset(buf, 0, 4096);

	p = buf;

	/* EUI64 */
	*p++ = 1;
	*p++ = 8;
	p += 2;
	be64enc(p, dev->eui64);
	return (true);
}

bool
device_namespace_data(uint32_t nsid, struct nvme_namespace_data *nsdata)
{
	struct backing_device *dev;

	dev = lookup_device(nsid);
	if (dev == NULL)
		return (false);

	memset(nsdata, 0, sizeof(*nsdata));
	nsdata->nsze = htole64(dev->nlbas);
	nsdata->ncap = nsdata->nsze;
	nsdata->nuse = nsdata->ncap;
	nsdata->nlbaf = 1 - 1;
	nsdata->flbas = NVMEF(NVME_NS_DATA_FLBAS_FORMAT, 0);
	nsdata->lbaf[0] = NVMEF(NVME_NS_DATA_LBAF_LBADS,
	    ffs(dev->sector_size) - 1);

	be64enc(nsdata->eui64, dev->eui64);
	return (true);
}

static bool
read_buffer(int fd, void *buf, size_t len, off_t offset)
{
	ssize_t nread;
	char *dst;

	dst = buf;
	while (len > 0) {
		nread = pread(fd, dst, len, offset);
		if (nread == -1 && errno == EINTR)
			continue;
		if (nread <= 0)
			return (false);
		dst += nread;
		len -= nread;
		offset += nread;
	}
	return (true);
}

void
device_read(uint32_t nsid, uint64_t lba, u_int nlb,
    const struct nvmf_capsule *nc)
{
	struct backing_device *dev;
	char *p, *src;
	off_t off;
	size_t len;

	dev = lookup_device(nsid);
	if (dev == NULL) {
		nvmf_send_generic_error(nc,
		    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
		return;
	}

	if (lba + nlb < lba || lba + nlb > dev->nlbas) {
		nvmf_send_generic_error(nc, NVME_SC_LBA_OUT_OF_RANGE);
		return;
	}

	off = lba * dev->sector_size;
	len = nlb * dev->sector_size;
	if (nvmf_capsule_data_len(nc) != len) {
		nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
		return;
	}

	if (dev->type == RAMDISK) {
		p = NULL;
		src = (char *)dev->mem + off;
	} else {
		p = malloc(len);
		if (!read_buffer(dev->fd, p, len, off)) {
			free(p);
			nvmf_send_generic_error(nc,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
			return;
		}
		src = p;
	}

	nvmf_send_controller_data(nc, src, len);
	free(p);
}

static bool
write_buffer(int fd, const void *buf, size_t len, off_t offset)
{
	ssize_t nwritten;
	const char *src;

	src = buf;
	while (len > 0) {
		nwritten = pwrite(fd, src, len, offset);
		if (nwritten == -1 && errno == EINTR)
			continue;
		if (nwritten <= 0)
			return (false);
		src += nwritten;
		len -= nwritten;
		offset += nwritten;
	}
	return (true);
}

void
device_write(uint32_t nsid, uint64_t lba, u_int nlb,
    const struct nvmf_capsule *nc)
{
	struct backing_device *dev;
	char *p, *dst;
	off_t off;
	size_t len;
	int error;

	dev = lookup_device(nsid);
	if (dev == NULL) {
		nvmf_send_generic_error(nc,
		    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
		return;
	}

	if (lba + nlb < lba || lba + nlb > dev->nlbas) {
		nvmf_send_generic_error(nc, NVME_SC_LBA_OUT_OF_RANGE);
		return;
	}

	off = lba * dev->sector_size;
	len = nlb * dev->sector_size;
	if (nvmf_capsule_data_len(nc) != len) {
		nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
		return;
	}

	if (dev->type == RAMDISK) {
		p = NULL;
		dst = (char *)dev->mem + off;
	} else {
		p = malloc(len);
		dst = p;
	}

	error = nvmf_receive_controller_data(nc, 0, dst, len);
	if (error != 0) {
		nvmf_send_generic_error(nc, NVME_SC_TRANSIENT_TRANSPORT_ERROR);
		free(p);
		return;
	}

	if (dev->type != RAMDISK) {
		if (!write_buffer(dev->fd, p, len, off)) {
			free(p);
			nvmf_send_generic_error(nc,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
			return;
		}
	}
	free(p);
	nvmf_send_success(nc);
}

void
device_flush(uint32_t nsid, const struct nvmf_capsule *nc)
{
	struct backing_device *dev;

	dev = lookup_device(nsid);
	if (dev == NULL) {
		nvmf_send_generic_error(nc,
		    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
		return;
	}

	switch (dev->type) {
	case RAMDISK:
		break;
	case FILE:
		if (fdatasync(dev->fd) == -1) {
			nvmf_send_error(nc, NVME_SCT_MEDIA_ERROR,
			    NVME_SC_WRITE_FAULTS);
			return;
		}
		break;
	case CDEV:
		if (ioctl(dev->fd, DIOCGFLUSH) == -1) {
			nvmf_send_error(nc, NVME_SCT_MEDIA_ERROR,
			    NVME_SC_WRITE_FAULTS);
			return;
		}
	}

	nvmf_send_success(nc);
}
