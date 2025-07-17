/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/disk.h>
#include <sys/queue.h>
#include <stand.h>
#include <stdarg.h>
#include <bootstrap.h>
#include <part.h>
#include <assert.h>

#include "disk.h"

#ifdef DISK_DEBUG
# define DPRINTF(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DPRINTF(fmt, args...)	((void)0)
#endif

struct open_disk {
	struct ptable		*table;
	uint64_t		mediasize;
	uint64_t		entrysize;
	u_int			sectorsize;
};

struct print_args {
	struct disk_devdesc	*dev;
	const char		*prefix;
	int			verbose;
};

/* Convert size to a human-readable number. */
static char *
display_size(uint64_t size, u_int sectorsize)
{
	static char buf[80];
	char unit;

	size = size * sectorsize / 1024;
	unit = 'K';
	if (size >= 10485760000LL) {
		size /= 1073741824;
		unit = 'T';
	} else if (size >= 10240000) {
		size /= 1048576;
		unit = 'G';
	} else if (size >= 10000) {
		size /= 1024;
		unit = 'M';
	}
	snprintf(buf, sizeof(buf), "%4ld%cB", (long)size, unit);
	return (buf);
}

int
ptblread(void *d, void *buf, size_t blocks, uint64_t offset)
{
	struct disk_devdesc *dev;
	struct open_disk *od;

	dev = (struct disk_devdesc *)d;
	od = (struct open_disk *)dev->dd.d_opendata;

	/*
	 * The strategy function assumes the offset is in units of 512 byte
	 * sectors. For larger sector sizes, we need to adjust the offset to
	 * match the actual sector size.
	 */
	offset *= (od->sectorsize / 512);
	/*
	 * As the GPT backup partition is located at the end of the disk,
	 * to avoid reading past disk end, flag bcache not to use RA.
	 */
	return (dev->dd.d_dev->dv_strategy(dev, F_READ | F_NORA, offset,
	    blocks * od->sectorsize, (char *)buf, NULL));
}

static int
ptable_print(void *arg, const char *pname, const struct ptable_entry *part)
{
	struct disk_devdesc dev;
	struct print_args *pa, bsd;
	struct open_disk *od;
	struct ptable *table;
	char line[80];
	int res;
	u_int sectsize;
	uint64_t partsize;

	pa = (struct print_args *)arg;
	od = (struct open_disk *)pa->dev->dd.d_opendata;
	sectsize = od->sectorsize;
	partsize = part->end - part->start + 1;
	snprintf(line, sizeof(line), "  %s%s: %s", pa->prefix, pname,
	    parttype2str(part->type));
	if (pager_output(line))
		return (1);

	if (pa->verbose) {
		/* Emit extra tab when the line is shorter than 3 tab stops */
		if (strlen(line) < 24)
			(void) pager_output("\t");

		snprintf(line, sizeof(line), "\t%s",
		    display_size(partsize, sectsize));
		if (pager_output(line))
			return (1);
	}
	if (pager_output("\n"))
		return (1);

	res = 0;
	if (part->type == PART_FREEBSD) {
		/* Open slice with BSD label */
		dev.dd.d_dev = pa->dev->dd.d_dev;
		dev.dd.d_unit = pa->dev->dd.d_unit;
		dev.d_slice = part->index;
		dev.d_partition = D_PARTNONE;
		if (disk_open(&dev, partsize, sectsize) == 0) {
			table = ptable_open(&dev, partsize, sectsize, ptblread);
			if (table != NULL) {
				snprintf(line, sizeof(line), "  %s%s",
				    pa->prefix, pname);
				bsd.dev = pa->dev;
				bsd.prefix = line;
				bsd.verbose = pa->verbose;
				res = ptable_iterate(table, &bsd, ptable_print);
				ptable_close(table);
			}
			disk_close(&dev);
		}
	}

	return (res);
}

int
disk_print(struct disk_devdesc *dev, char *prefix, int verbose)
{
	struct open_disk *od;
	struct print_args pa;

	/* Disk should be opened */
	od = (struct open_disk *)dev->dd.d_opendata;
	pa.dev = dev;
	pa.prefix = prefix;
	pa.verbose = verbose;
	return (ptable_iterate(od->table, &pa, ptable_print));
}

int
disk_read(struct disk_devdesc *dev, void *buf, uint64_t offset, u_int blocks)
{
	struct open_disk *od;
	int ret;

	od = (struct open_disk *)dev->dd.d_opendata;
	ret = dev->dd.d_dev->dv_strategy(dev, F_READ, dev->d_offset + offset,
	    blocks * od->sectorsize, buf, NULL);

	return (ret);
}

int
disk_write(struct disk_devdesc *dev, void *buf, uint64_t offset, u_int blocks)
{
	struct open_disk *od;
	int ret;

	od = (struct open_disk *)dev->dd.d_opendata;
	ret = dev->dd.d_dev->dv_strategy(dev, F_WRITE, dev->d_offset + offset,
	    blocks * od->sectorsize, buf, NULL);

	return (ret);
}

int
disk_ioctl(struct disk_devdesc *dev, u_long cmd, void *data)
{
	struct open_disk *od = dev->dd.d_opendata;

	if (od == NULL)
		return (ENOTTY);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = od->sectorsize;
		break;
	case DIOCGMEDIASIZE:
		if (dev->d_offset == 0)
			*(uint64_t *)data = od->mediasize;
		else
			*(uint64_t *)data = od->entrysize * od->sectorsize;
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
disk_open(struct disk_devdesc *dev, uint64_t mediasize, u_int sectorsize)
{
	struct disk_devdesc partdev;
	struct open_disk *od;
	struct ptable *table;
	struct ptable_entry part;
	int rc, slice, partition;

	if (sectorsize == 0) {
		DPRINTF("unknown sector size");
		return (ENXIO);
	}
	rc = 0;
	od = (struct open_disk *)malloc(sizeof(struct open_disk));
	if (od == NULL) {
		DPRINTF("no memory");
		return (ENOMEM);
	}
	dev->dd.d_opendata = od;
	od->entrysize = 0;
	od->mediasize = mediasize;
	od->sectorsize = sectorsize;
	/*
	 * While we are reading disk metadata, make sure we do it relative
	 * to the start of the disk
	 */
	memcpy(&partdev, dev, sizeof(partdev));
	partdev.d_offset = 0;
	partdev.d_slice = D_SLICENONE;
	partdev.d_partition = D_PARTNONE;

	dev->d_offset = 0;
	table = NULL;
	slice = dev->d_slice;
	partition = dev->d_partition;

	DPRINTF("%s unit %d, slice %d, partition %d => %p", disk_fmtdev(dev),
	    dev->dd.d_unit, dev->d_slice, dev->d_partition, od);

	/* Determine disk layout. */
	od->table = ptable_open(&partdev, mediasize / sectorsize, sectorsize,
	    ptblread);
	if (od->table == NULL) {
		DPRINTF("Can't read partition table");
		rc = ENXIO;
		goto out;
	}

	if (ptable_getsize(od->table, &mediasize) != 0) {
		rc = ENXIO;
		goto out;
	}
	od->mediasize = mediasize;

	if (ptable_gettype(od->table) == PTABLE_BSD &&
	    partition >= 0) {
		/* It doesn't matter what value has d_slice */
		rc = ptable_getpart(od->table, &part, partition);
		if (rc == 0) {
			dev->d_offset = part.start;
			od->entrysize = part.end - part.start + 1;
		}
	} else if (ptable_gettype(od->table) == PTABLE_ISO9660) {
		dev->d_offset = 0;
		od->entrysize = mediasize;
	} else if (slice >= 0) {
		/* Try to get information about partition */
		if (slice == 0)
			rc = ptable_getbestpart(od->table, &part);
		else
			rc = ptable_getpart(od->table, &part, slice);
		if (rc != 0) /* Partition doesn't exist */
			goto out;
		dev->d_offset = part.start;
		od->entrysize = part.end - part.start + 1;
		slice = part.index;
		if (ptable_gettype(od->table) == PTABLE_GPT) {
			partition = D_PARTISGPT;
			goto out; /* Nothing more to do */
		} else if (partition == D_PARTISGPT) {
			/*
			 * When we try to open GPT partition, but partition
			 * table isn't GPT, reset partition value to
			 * D_PARTWILD and try to autodetect appropriate value.
			 */
			partition = D_PARTWILD;
		}

		/*
		 * If partition is D_PARTNONE, then disk_open() was called
		 * to open raw MBR slice.
		 */
		if (partition == D_PARTNONE)
			goto out;

		/*
		 * If partition is D_PARTWILD and we are looking at a BSD slice,
		 * then try to read BSD label, otherwise return the
		 * whole MBR slice.
		 */
		if (partition == D_PARTWILD &&
		    part.type != PART_FREEBSD)
			goto out;
		/* Try to read BSD label */
		table = ptable_open(dev, part.end - part.start + 1,
		    od->sectorsize, ptblread);
		if (table == NULL) {
			DPRINTF("Can't read BSD label");
			rc = ENXIO;
			goto out;
		}
		/*
		 * If slice contains BSD label and partition < 0, then
		 * assume the 'a' partition. Otherwise just return the
		 * whole MBR slice, because it can contain ZFS.
		 */
		if (partition < 0) {
			if (ptable_gettype(table) != PTABLE_BSD)
				goto out;
			partition = 0;
		}
		rc = ptable_getpart(table, &part, partition);
		if (rc != 0)
			goto out;
		dev->d_offset += part.start;
		od->entrysize = part.end - part.start + 1;
	}
out:
	if (table != NULL)
		ptable_close(table);

	if (rc != 0) {
		if (od->table != NULL)
			ptable_close(od->table);
		free(od);
		DPRINTF("%s could not open", disk_fmtdev(dev));
	} else {
		/* Save the slice and partition number to the dev */
		dev->d_slice = slice;
		dev->d_partition = partition;
		DPRINTF("%s offset %lld => %p", disk_fmtdev(dev),
		    (long long)dev->d_offset, od);
	}
	return (rc);
}

int
disk_close(struct disk_devdesc *dev)
{
	struct open_disk *od;

	od = (struct open_disk *)dev->dd.d_opendata;
	DPRINTF("%s closed => %p", disk_fmtdev(dev), od);
	ptable_close(od->table);
	free(od);
	return (0);
}

char *
disk_fmtdev(struct devdesc *vdev)
{
	struct disk_devdesc *dev = (struct disk_devdesc *)vdev;
	static char buf[128];
	char *cp;

	assert(vdev->d_dev->dv_type == DEVT_DISK);
	cp = buf + sprintf(buf, "%s%d", dev->dd.d_dev->dv_name, dev->dd.d_unit);
	if (dev->d_slice > D_SLICENONE) {
#ifdef LOADER_GPT_SUPPORT
		if (dev->d_partition == D_PARTISGPT) {
			sprintf(cp, "p%d:", dev->d_slice);
			return (buf);
		} else
#endif
#ifdef LOADER_MBR_SUPPORT
			cp += sprintf(cp, "s%d", dev->d_slice);
#endif
	}
	if (dev->d_partition > D_PARTNONE)
		cp += sprintf(cp, "%c", dev->d_partition + 'a');
	strcat(cp, ":");
	return (buf);
}

int
disk_parsedev(struct devdesc **idev, const char *devspec, const char **path)
{
	int unit, slice, partition;
	const char *np;
	char *cp;
	struct disk_devdesc *dev;

	np = devspec + 4;	/* Skip the leading 'disk' */
	unit = -1;
	/*
	 * If there is path/file info after the device info, then any missing
	 * slice or partition info should be considered a request to search for
	 * an appropriate partition.  Otherwise we want to open the raw device
	 * itself and not try to fill in missing info by searching.
	 */
	if ((cp = strchr(np, ':')) != NULL && cp[1] != '\0') {
		slice = D_SLICEWILD;
		partition = D_PARTWILD;
	} else {
		slice = D_SLICENONE;
		partition = D_PARTNONE;
	}

	if (*np != '\0' && *np != ':') {
		unit = strtol(np, &cp, 10);
		if (cp == np)
			return (EUNIT);
#ifdef LOADER_GPT_SUPPORT
		if (*cp == 'p') {
			np = cp + 1;
			slice = strtol(np, &cp, 10);
			if (np == cp)
				return (ESLICE);
			/* we don't support nested partitions on GPT */
			if (*cp != '\0' && *cp != ':')
				return (EINVAL);
			partition = D_PARTISGPT;
		} else
#endif
#ifdef LOADER_MBR_SUPPORT
		if (*cp == 's') {
			np = cp + 1;
			slice = strtol(np, &cp, 10);
			if (np == cp)
				return (ESLICE);
		}
#endif
		if (*cp != '\0' && *cp != ':') {
			partition = *cp - 'a';
			if (partition < 0)
				return (EPART);
			cp++;
		}
	} else
		return (EINVAL);

	if (*cp != '\0' && *cp != ':')
		return (EINVAL);
	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return (ENOMEM);
	dev->dd.d_unit = unit;
	dev->d_slice = slice;
	dev->d_partition = partition;
	*idev = &dev->dd;
	if (path != NULL)
		*path = (*cp == '\0') ? cp: cp + 1;
	return (0);
}
