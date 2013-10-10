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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/disk.h>
#include <sys/queue.h>
#include <stand.h>
#include <stdarg.h>
#include <bootstrap.h>
#include <part.h>

#include "disk.h"

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

struct open_disk {
	struct ptable		*table;
	off_t			mediasize;
	u_int			sectorsize;
	u_int			flags;
	int			rcnt;
};

struct print_args {
	struct disk_devdesc	*dev;
	const char		*prefix;
	int			verbose;
};

struct dentry {
	const struct devsw	*d_dev;
	int			d_unit;
	int			d_slice;
	int			d_partition;

	struct open_disk	*od;
	off_t			d_offset;
	STAILQ_ENTRY(dentry)	entry;
#ifdef DISK_DEBUG
	uint32_t		count;
#endif
};

static STAILQ_HEAD(, dentry) opened_disks =
    STAILQ_HEAD_INITIALIZER(opened_disks);

static int
disk_lookup(struct disk_devdesc *dev)
{
	struct dentry *entry;
	int rc;

	rc = ENOENT;
	STAILQ_FOREACH(entry, &opened_disks, entry) {
		if (entry->d_dev != dev->d_dev ||
		    entry->d_unit != dev->d_unit)
			continue;
		dev->d_opendata = entry->od;
		if (entry->d_slice == dev->d_slice &&
		    entry->d_partition == dev->d_partition) {
			dev->d_offset = entry->d_offset;
			DEBUG("%s offset %lld", disk_fmtdev(dev),
			    dev->d_offset);
#ifdef DISK_DEBUG
			entry->count++;
#endif
			return (0);
		}
		rc = EAGAIN;
	}
	return (rc);
}

static void
disk_insert(struct disk_devdesc *dev)
{
	struct dentry *entry;

	entry = (struct dentry *)malloc(sizeof(struct dentry));
	if (entry == NULL) {
		DEBUG("no memory");
		return;
	}
	entry->d_dev = dev->d_dev;
	entry->d_unit = dev->d_unit;
	entry->d_slice = dev->d_slice;
	entry->d_partition = dev->d_partition;
	entry->od = (struct open_disk *)dev->d_opendata;
	entry->od->rcnt++;
	entry->d_offset = dev->d_offset;
#ifdef DISK_DEBUG
	entry->count = 1;
#endif
	STAILQ_INSERT_TAIL(&opened_disks, entry, entry);
	DEBUG("%s cached", disk_fmtdev(dev));
}

#ifdef DISK_DEBUG
COMMAND_SET(dcachestat, "dcachestat", "get disk cache stats",
    command_dcachestat);

static int
command_dcachestat(int argc, char *argv[])
{
	struct disk_devdesc dev;
	struct dentry *entry;

	STAILQ_FOREACH(entry, &opened_disks, entry) {
		dev.d_dev = (struct devsw *)entry->d_dev;
		dev.d_unit = entry->d_unit;
		dev.d_slice = entry->d_slice;
		dev.d_partition = entry->d_partition;
		printf("%s %d => %p [%d]\n", disk_fmtdev(&dev), entry->count,
		    entry->od, entry->od->rcnt);
	}
	return (CMD_OK);
}
#endif /* DISK_DEBUG */

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
	sprintf(buf, "%ld%cB", (long)size, unit);
	return (buf);
}

static int
ptblread(void *d, void *buf, size_t blocks, off_t offset)
{
	struct disk_devdesc *dev;
	struct open_disk *od;

	dev = (struct disk_devdesc *)d;
	od = (struct open_disk *)dev->d_opendata;
	return (dev->d_dev->dv_strategy(dev, F_READ, offset,
	    blocks * od->sectorsize, (char *)buf, NULL));
}

#define	PWIDTH	35
static void
ptable_print(void *arg, const char *pname, const struct ptable_entry *part)
{
	struct print_args *pa, bsd;
	struct open_disk *od;
	struct ptable *table;
	char line[80];

	pa = (struct print_args *)arg;
	od = (struct open_disk *)pa->dev->d_opendata;
	sprintf(line, "  %s%s: %s", pa->prefix, pname,
	    parttype2str(part->type));
	if (pa->verbose)
		sprintf(line, "%-*s%s", PWIDTH, line,
		    display_size(part->end - part->start + 1,
		    od->sectorsize));
	strcat(line, "\n");
	pager_output(line);
	if (part->type == PART_FREEBSD) {
		/* Open slice with BSD label */
		pa->dev->d_offset = part->start;
		table = ptable_open(pa->dev, part->end - part->start + 1,
		    od->sectorsize, ptblread);
		if (table == NULL)
			return;
		sprintf(line, "  %s%s", pa->prefix, pname);
		bsd.dev = pa->dev;
		bsd.prefix = line;
		bsd.verbose = pa->verbose;
		ptable_iterate(table, &bsd, ptable_print);
		ptable_close(table);
	}
}
#undef PWIDTH

void
disk_print(struct disk_devdesc *dev, char *prefix, int verbose)
{
	struct open_disk *od;
	struct print_args pa;

	/* Disk should be opened */
	od = (struct open_disk *)dev->d_opendata;
	pa.dev = dev;
	pa.prefix = prefix;
	pa.verbose = verbose;
	ptable_iterate(od->table, &pa, ptable_print);
}

int
disk_open(struct disk_devdesc *dev, off_t mediasize, u_int sectorsize,
    u_int flags)
{
	struct open_disk *od;
	struct ptable *table;
	struct ptable_entry part;
	int rc, slice, partition;

	rc = 0;
	if ((flags & DISK_F_NOCACHE) == 0) {
		rc = disk_lookup(dev);
		if (rc == 0)
			return (0);
	}
	/*
	 * While we are reading disk metadata, make sure we do it relative
	 * to the start of the disk
	 */
	dev->d_offset = 0;
	table = NULL;
	slice = dev->d_slice;
	partition = dev->d_partition;
	if (rc == EAGAIN) {
		/*
		 * This entire disk was already opened and there is no
		 * need to allocate new open_disk structure and open the
		 * main partition table.
		 */
		od = (struct open_disk *)dev->d_opendata;
		DEBUG("%s unit %d, slice %d, partition %d => %p (cached)",
		    disk_fmtdev(dev), dev->d_unit, dev->d_slice,
		    dev->d_partition, od);
		goto opened;
	} else {
		od = (struct open_disk *)malloc(sizeof(struct open_disk));
		if (od == NULL) {
			DEBUG("no memory");
			return (ENOMEM);
		}
		dev->d_opendata = od;
		od->rcnt = 0;
	}
	od->mediasize = mediasize;
	od->sectorsize = sectorsize;
	od->flags = flags;
	DEBUG("%s unit %d, slice %d, partition %d => %p",
	    disk_fmtdev(dev), dev->d_unit, dev->d_slice, dev->d_partition, od);

	/* Determine disk layout. */
	od->table = ptable_open(dev, mediasize / sectorsize, sectorsize,
	    ptblread);
	if (od->table == NULL) {
		DEBUG("Can't read partition table");
		rc = ENXIO;
		goto out;
	}
opened:
	rc = 0;
	if (ptable_gettype(od->table) == PTABLE_BSD &&
	    partition >= 0) {
		/* It doesn't matter what value has d_slice */
		rc = ptable_getpart(od->table, &part, partition);
		if (rc == 0)
			dev->d_offset = part.start;
	} else if (slice >= 0) {
		/* Try to get information about partition */
		if (slice == 0)
			rc = ptable_getbestpart(od->table, &part);
		else
			rc = ptable_getpart(od->table, &part, slice);
		if (rc != 0) /* Partition doesn't exist */
			goto out;
		dev->d_offset = part.start;
		slice = part.index;
		if (ptable_gettype(od->table) == PTABLE_GPT) {
			partition = 255;
			goto out; /* Nothing more to do */
		} else if (partition == 255) {
			/*
			 * When we try to open GPT partition, but partition
			 * table isn't GPT, reset d_partition value to -1
			 * and try to autodetect appropriate value.
			 */
			partition = -1;
		}
		/*
		 * If d_partition < 0 and we are looking at a BSD slice,
		 * then try to read BSD label, otherwise return the
		 * whole MBR slice.
		 */
		if (partition == -1 &&
		    part.type != PART_FREEBSD)
			goto out;
		/* Try to read BSD label */
		table = ptable_open(dev, part.end - part.start + 1,
		    od->sectorsize, ptblread);
		if (table == NULL) {
			DEBUG("Can't read BSD label");
			rc = ENXIO;
			goto out;
		}
		/*
		 * If slice contains BSD label and d_partition < 0, then
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
	}
out:
	if (table != NULL)
		ptable_close(table);

	if (rc != 0) {
		if (od->rcnt < 1) {
			if (od->table != NULL)
				ptable_close(od->table);
			free(od);
		}
		DEBUG("%s could not open", disk_fmtdev(dev));
	} else {
		if ((flags & DISK_F_NOCACHE) == 0)
			disk_insert(dev);
		/* Save the slice and partition number to the dev */
		dev->d_slice = slice;
		dev->d_partition = partition;
		DEBUG("%s offset %lld => %p", disk_fmtdev(dev),
		    dev->d_offset, od);
	}
	return (rc);
}

int
disk_close(struct disk_devdesc *dev)
{
	struct open_disk *od;

	od = (struct open_disk *)dev->d_opendata;
	DEBUG("%s closed => %p [%d]", disk_fmtdev(dev), od, od->rcnt);
	if (od->flags & DISK_F_NOCACHE) {
		ptable_close(od->table);
		free(od);
	}
	return (0);
}

void
disk_cleanup(const struct devsw *d_dev)
{
#ifdef DISK_DEBUG
	struct disk_devdesc dev;
#endif
	struct dentry *entry, *tmp;

	STAILQ_FOREACH_SAFE(entry, &opened_disks, entry, tmp) {
		if (entry->d_dev != d_dev)
			continue;
		entry->od->rcnt--;
#ifdef DISK_DEBUG
		dev.d_dev = (struct devsw *)entry->d_dev;
		dev.d_unit = entry->d_unit;
		dev.d_slice = entry->d_slice;
		dev.d_partition = entry->d_partition;
		DEBUG("%s was freed => %p [%d]", disk_fmtdev(&dev),
		    entry->od, entry->od->rcnt);
#endif
		STAILQ_REMOVE(&opened_disks, entry, dentry, entry);
		if (entry->od->rcnt < 1) {
			if (entry->od->table != NULL)
				ptable_close(entry->od->table);
			free(entry->od);
		}
		free(entry);
	}
}

char*
disk_fmtdev(struct disk_devdesc *dev)
{
	static char buf[128];
	char *cp;

	cp = buf + sprintf(buf, "%s%d", dev->d_dev->dv_name, dev->d_unit);
	if (dev->d_slice >= 0) {
#ifdef LOADER_GPT_SUPPORT
		if (dev->d_partition == 255) {
			sprintf(cp, "p%d:", dev->d_slice);
			return (buf);
		} else
#endif
#ifdef LOADER_MBR_SUPPORT
			cp += sprintf(cp, "s%d", dev->d_slice);
#endif
	}
	if (dev->d_partition >= 0)
		cp += sprintf(cp, "%c", dev->d_partition + 'a');
	strcat(cp, ":");
	return (buf);
}

int
disk_parsedev(struct disk_devdesc *dev, const char *devspec, const char **path)
{
	int unit, slice, partition;
	const char *np;
	char *cp;

	np = devspec;
	unit = slice = partition = -1;
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
			partition = 255;
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
	dev->d_unit = unit;
	dev->d_slice = slice;
	dev->d_partition = partition;
	if (path != NULL)
		*path = (*cp == '\0') ? cp: cp + 1;
	return (0);
}
