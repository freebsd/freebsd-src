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

/*
 * BIOS disk device handling.
 * 
 * Ideas and algorithms from:
 *
 * - NetBSD libi386/biosdisk.c
 * - FreeBSD biosboot/disk.c
 *
 */

#include <sys/disk.h>
#include <sys/limits.h>
#include <stand.h>
#include <machine/bootinfo.h>
#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include <edd.h>
#include "disk.h"
#include "libi386.h"

#ifdef LOADER_GELI_SUPPORT
#include "cons.h"
#include "drv.h"
#include "gpt.h"
#include "part.h"
#include <uuid.h>
struct pentry {
	struct ptable_entry	part;
	uint64_t		flags;
	union {
		uint8_t bsd;
		uint8_t	mbr;
		uuid_t	gpt;
		uint16_t vtoc8;
	} type;
	STAILQ_ENTRY(pentry)	entry;
};
struct ptable {
	enum ptable_type	type;
	uint16_t		sectorsize;
	uint64_t		sectors;

	STAILQ_HEAD(, pentry)	entries;
};

#include "geliboot.c"
#endif /* LOADER_GELI_SUPPORT */

CTASSERT(sizeof(struct i386_devdesc) >= sizeof(struct disk_devdesc));

#define BIOS_NUMDRIVES		0x475
#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)

#define DT_ATAPI		0x10		/* disk type for ATAPI floppies */
#define WDMAJOR			0		/* major numbers for devices we frontend for */
#define WFDMAJOR		1
#define FDMAJOR			2
#define DAMAJOR			4

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bdinfo
{
	int		bd_unit;	/* BIOS unit number */
	int		bd_cyl;		/* BIOS geometry */
	int		bd_hds;
	int		bd_sec;
	int		bd_flags;
#define	BD_MODEINT13	0x0000
#define	BD_MODEEDD1	0x0001
#define	BD_MODEEDD3	0x0002
#define	BD_MODEMASK	0x0003
#define	BD_FLOPPY	0x0004
	int		bd_type;	/* BIOS 'drive type' (floppy only) */
	uint16_t	bd_sectorsize;	/* Sector size */
	uint64_t	bd_sectors;	/* Disk size */
	int		bd_open;	/* reference counter */
	void		*bd_bcache;	/* buffer cache data */
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

#define	BD(dev)		(bdinfo[(dev)->dd.d_unit])

static int bd_read(struct disk_devdesc *dev, daddr_t dblk, int blks,
    caddr_t dest);
static int bd_write(struct disk_devdesc *dev, daddr_t dblk, int blks,
    caddr_t dest);
static int bd_int13probe(struct bdinfo *bd);

static int bd_init(void);
static int bd_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
    char *buf, size_t *rsize);
static int bd_realstrategy(void *devdata, int flag, daddr_t dblk, size_t size,
    char *buf, size_t *rsize);
static int bd_open(struct open_file *f, ...);
static int bd_close(struct open_file *f);
static int bd_ioctl(struct open_file *f, u_long cmd, void *data);
static int bd_print(int verbose);

#ifdef LOADER_GELI_SUPPORT
enum isgeli {
	ISGELI_UNKNOWN,
	ISGELI_NO,
	ISGELI_YES
};
static enum isgeli geli_status[MAXBDDEV][MAXTBLENTS];

int bios_read(void *, void *, off_t off, void *buf, size_t bytes);
#endif /* LOADER_GELI_SUPPORT */

struct devsw biosdisk = {
	"disk",
	DEVT_DISK,
	bd_init,
	bd_strategy,
	bd_open,
	bd_close,
	bd_ioctl,
	bd_print,
	NULL
};

/*
 * Translate between BIOS device numbers and our private unit numbers.
 */
int
bd_bios2unit(int biosdev)
{
	int i;

	DEBUG("looking for bios device 0x%x", biosdev);
	for (i = 0; i < nbdinfo; i++) {
		DEBUG("bd unit %d is BIOS device 0x%x", i, bdinfo[i].bd_unit);
		if (bdinfo[i].bd_unit == biosdev)
			return (i);
	}
	return (-1);
}

int
bd_unit2bios(int unit)
{

	if ((unit >= 0) && (unit < nbdinfo))
		return (bdinfo[unit].bd_unit);
	return (-1);
}

/*
 * Quiz the BIOS for disk devices, save a little info about them.
 */
static int
bd_init(void)
{
	int base, unit, nfd = 0;

#ifdef LOADER_GELI_SUPPORT
	geli_init();
#endif
	/* sequence 0, 0x80 */
	for (base = 0; base <= 0x80; base += 0x80) {
		for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
#ifndef VIRTUALBOX
			/*
			 * Check the BIOS equipment list for number
			 * of fixed disks.
			 */
			if(base == 0x80 &&
			    (nfd >= *(unsigned char *)PTOV(BIOS_NUMDRIVES)))
				break;
#endif
			bdinfo[nbdinfo].bd_open = 0;
			bdinfo[nbdinfo].bd_bcache = NULL;
			bdinfo[nbdinfo].bd_unit = unit;
			bdinfo[nbdinfo].bd_flags = unit < 0x80 ? BD_FLOPPY: 0;
			if (!bd_int13probe(&bdinfo[nbdinfo]))
				break;

			/* XXX we need "disk aliases" to make this simpler */
			printf("BIOS drive %c: is disk%d\n", (unit < 0x80) ?
			    ('A' + unit): ('C' + unit - 0x80), nbdinfo);
			nbdinfo++;
			if (base == 0x80)
				nfd++;
		}
	}
	bcache_add_dev(nbdinfo);
	return(0);
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */
static int
bd_int13probe(struct bdinfo *bd)
{
	struct edd_params params;
	int ret = 1;	/* assume success */

	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x800;
	v86.edx = bd->bd_unit;
	v86int();

	/* Don't error out if we get bad sector number, try EDD as well */
	if (V86_CY(v86.efl) ||	/* carry set */
	    (v86.edx & 0xff) <= (unsigned)(bd->bd_unit & 0x7f))	/* unit # bad */
		return (0);	/* skip device */

	if ((v86.ecx & 0x3f) == 0) /* absurd sector number */
		ret = 0;	/* set error */

	/* Convert max cyl # -> # of cylinders */
	bd->bd_cyl = ((v86.ecx & 0xc0) << 2) + ((v86.ecx & 0xff00) >> 8) + 1;
	/* Convert max head # -> # of heads */
	bd->bd_hds = ((v86.edx & 0xff00) >> 8) + 1;
	bd->bd_sec = v86.ecx & 0x3f;
	bd->bd_type = v86.ebx & 0xff;
	bd->bd_flags |= BD_MODEINT13;

	/* Calculate sectors count from the geometry */
	bd->bd_sectors = bd->bd_cyl * bd->bd_hds * bd->bd_sec;
	bd->bd_sectorsize = BIOSDISK_SECSIZE;
	DEBUG("unit 0x%x geometry %d/%d/%d", bd->bd_unit, bd->bd_cyl,
	    bd->bd_hds, bd->bd_sec);

	/* Determine if we can use EDD with this device. */
	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4100;
	v86.edx = bd->bd_unit;
	v86.ebx = 0x55aa;
	v86int();
	if (V86_CY(v86.efl) ||	/* carry set */
	    (v86.ebx & 0xffff) != 0xaa55 || /* signature */
	    (v86.ecx & EDD_INTERFACE_FIXED_DISK) == 0)
		return (ret);	/* return code from int13 AH=08 */

	/* EDD supported */
	bd->bd_flags |= BD_MODEEDD1;
	if ((v86.eax & 0xff00) >= 0x3000)
		bd->bd_flags |= BD_MODEEDD3;
	/* Get disk params */
	params.len = sizeof(struct edd_params);
	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4800;
	v86.edx = bd->bd_unit;
	v86.ds = VTOPSEG(&params);
	v86.esi = VTOPOFF(&params);
	v86int();
	if (!V86_CY(v86.efl)) {
		uint64_t total;

		/*
		 * Sector size must be a multiple of 512 bytes.
		 * An alternate test would be to check power of 2,
		 * powerof2(params.sector_size).
		 */
		if (params.sector_size % BIOSDISK_SECSIZE)
			bd->bd_sectorsize = BIOSDISK_SECSIZE;
		else
			bd->bd_sectorsize = params.sector_size;

		total = bd->bd_sectorsize * params.sectors;
		if (params.sectors != 0) {
			/* Only update if we did not overflow. */
			if (total > params.sectors)
				bd->bd_sectors = params.sectors;
		}

		total = (uint64_t)params.cylinders *
		    params.heads * params.sectors_per_track;
		if (bd->bd_sectors < total)
			bd->bd_sectors = total;

		ret = 1;
	}
	DEBUG("unit 0x%x flags %x, sectors %llu, sectorsize %u",
	    bd->bd_unit, bd->bd_flags, bd->bd_sectors, bd->bd_sectorsize);
	return (ret);
}

/*
 * Print information about disks
 */
static int
bd_print(int verbose)
{
	static char line[80];
	struct disk_devdesc dev;
	int i, ret = 0;

	if (nbdinfo == 0)
		return (0);

	printf("%s devices:", biosdisk.dv_name);
	if ((ret = pager_output("\n")) != 0)
		return (ret);

	for (i = 0; i < nbdinfo; i++) {
		snprintf(line, sizeof(line),
		    "    disk%d:   BIOS drive %c (%ju X %u):\n", i,
		    (bdinfo[i].bd_unit < 0x80) ? ('A' + bdinfo[i].bd_unit):
		    ('C' + bdinfo[i].bd_unit - 0x80),
		    (uintmax_t)bdinfo[i].bd_sectors,
		    bdinfo[i].bd_sectorsize);
		if ((ret = pager_output(line)) != 0)
			break;
		dev.dd.d_dev = &biosdisk;
		dev.dd.d_unit = i;
		dev.d_slice = -1;
		dev.d_partition = -1;
		if (disk_open(&dev,
		    bdinfo[i].bd_sectorsize * bdinfo[i].bd_sectors,
		    bdinfo[i].bd_sectorsize) == 0) {
			snprintf(line, sizeof(line), "    disk%d", i);
			ret = disk_print(&dev, line, verbose);
			disk_close(&dev);
			if (ret != 0)
			    return (ret);
		}
	}
	return (ret);
}

/*
 * Attempt to open the disk described by (dev) for use by (f).
 *
 * Note that the philosophy here is "give them exactly what
 * they ask for".  This is necessary because being too "smart"
 * about what the user might want leads to complications.
 * (eg. given no slice or partition value, with a disk that is
 *  sliced - are they after the first BSD slice, or the DOS
 *  slice before it?)
 */
static int
bd_open(struct open_file *f, ...)
{
	struct disk_devdesc *dev, rdev;
	struct disk_devdesc disk;
	int err, g_err;
	va_list ap;
	uint64_t size;

	va_start(ap, f);
	dev = va_arg(ap, struct disk_devdesc *);
	va_end(ap);

	if (dev->dd.d_unit < 0 || dev->dd.d_unit >= nbdinfo)
		return (EIO);
	BD(dev).bd_open++;
	if (BD(dev).bd_bcache == NULL)
	    BD(dev).bd_bcache = bcache_allocate();

	/*
	 * Read disk size from partition.
	 * This is needed to work around buggy BIOS systems returning
	 * wrong (truncated) disk media size.
	 * During bd_probe() we tested if the mulitplication of bd_sectors
	 * would overflow so it should be safe to perform here.
	 */
	disk.dd.d_dev = dev->dd.d_dev;
	disk.dd.d_unit = dev->dd.d_unit;
	disk.d_slice = -1;
	disk.d_partition = -1;
	disk.d_offset = 0;
	if (disk_open(&disk, BD(dev).bd_sectors * BD(dev).bd_sectorsize,
	    BD(dev).bd_sectorsize) == 0) {

		if (disk_ioctl(&disk, DIOCGMEDIASIZE, &size) == 0) {
			size /= BD(dev).bd_sectorsize;
			if (size > BD(dev).bd_sectors)
				BD(dev).bd_sectors = size;
		}
		disk_close(&disk);
	}

	err = disk_open(dev, BD(dev).bd_sectors * BD(dev).bd_sectorsize,
	    BD(dev).bd_sectorsize);

#ifdef LOADER_GELI_SUPPORT
	static char gelipw[GELI_PW_MAXLEN];
	char *passphrase;

	if (err)
		return (err);

	/* if we already know there is no GELI, skip the rest */
	if (geli_status[dev->dd.d_unit][dev->d_slice] != ISGELI_UNKNOWN)
		return (err);

	struct dsk dskp;
	struct ptable *table = NULL;
	struct ptable_entry part;
	struct pentry *entry;
	int geli_part = 0;

	dskp.drive = bd_unit2bios(dev->dd.d_unit);
	dskp.type = dev->dd.d_dev->dv_type;
	dskp.unit = dev->dd.d_unit;
	dskp.slice = dev->d_slice;
	dskp.part = dev->d_partition;
	dskp.start = dev->d_offset;

	memcpy(&rdev, dev, sizeof(rdev));
	/* to read the GPT table, we need to read the first sector */
	rdev.d_offset = 0;
	/* We need the LBA of the end of the partition */
	table = ptable_open(&rdev, BD(dev).bd_sectors,
	    BD(dev).bd_sectorsize, ptblread);
	if (table == NULL) {
		DEBUG("Can't read partition table");
		/* soft failure, return the exit status of disk_open */
		return (err);
	}

	if (table->type == PTABLE_GPT)
		dskp.part = 255;

	STAILQ_FOREACH(entry, &table->entries, entry) {
		dskp.slice = entry->part.index;
		dskp.start = entry->part.start;
		if (is_geli(&dskp) == 0) {
			geli_status[dev->dd.d_unit][dskp.slice] = ISGELI_YES;
			return (0);
		}
		if (geli_taste(bios_read, &dskp,
		    entry->part.end - entry->part.start) == 0) {
			if (geli_havekey(&dskp) == 0) {
				geli_status[dev->dd.d_unit][dskp.slice] = ISGELI_YES;
				geli_part++;
				continue;
			}
			if ((passphrase = getenv("kern.geom.eli.passphrase"))
			    != NULL) {
				/* Use the cached passphrase */
				bcopy(passphrase, &gelipw, GELI_PW_MAXLEN);
			}
			if (geli_passphrase(gelipw, dskp.unit, 'p',
				    (dskp.slice > 0 ? dskp.slice : dskp.part),
				    &dskp) == 0) {
				setenv("kern.geom.eli.passphrase", gelipw, 1);
				bzero(gelipw, sizeof(gelipw));
				geli_status[dev->dd.d_unit][dskp.slice] = ISGELI_YES;
				geli_part++;
				continue;
			}
		} else
			geli_status[dev->dd.d_unit][dskp.slice] = ISGELI_NO;
	}

	/* none of the partitions on this disk have GELI */
	if (geli_part == 0) {
		/* found no GELI */
		geli_status[dev->dd.d_unit][dev->d_slice] = ISGELI_NO;
	}
#endif /* LOADER_GELI_SUPPORT */

	return (err);
}

static int
bd_close(struct open_file *f)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)f->f_devdata;
	BD(dev).bd_open--;
	if (BD(dev).bd_open == 0) {
	    bcache_free(BD(dev).bd_bcache);
	    BD(dev).bd_bcache = NULL;
	}
	return (disk_close(dev));
}

static int
bd_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct disk_devdesc *dev;
	int rc;

	dev = (struct disk_devdesc *)f->f_devdata;

	rc = disk_ioctl(dev, cmd, data);
	if (rc != ENOTTY)
		return (rc);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = BD(dev).bd_sectorsize;
		break;
	case DIOCGMEDIASIZE:
		*(uint64_t *)data = BD(dev).bd_sectors * BD(dev).bd_sectorsize;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static int
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
	struct bcache_devdata bcd;
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)devdata;
	bcd.dv_strategy = bd_realstrategy;
	bcd.dv_devdata = devdata;
	bcd.dv_cache = BD(dev).bd_bcache;
	return (bcache_strategy(&bcd, rw, dblk + dev->d_offset,
	    size, buf, rsize));
}

static int
bd_realstrategy(void *devdata, int rw, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
    struct disk_devdesc *dev = (struct disk_devdesc *)devdata;
    uint64_t		disk_blocks;
    int			blks, rc;
#ifdef BD_SUPPORT_FRAGS /* XXX: sector size */
    char		fragbuf[BIOSDISK_SECSIZE];
    size_t		fragsize;

    fragsize = size % BIOSDISK_SECSIZE;
#else
    if (size % BD(dev).bd_sectorsize)
	panic("bd_strategy: %d bytes I/O not multiple of block size", size);
#endif

    DEBUG("open_disk %p", dev);

    /*
     * Check the value of the size argument. We do have quite small
     * heap (64MB), but we do not know good upper limit, so we check against
     * INT_MAX here. This will also protect us against possible overflows
     * while translating block count to bytes.
     */
    if (size > INT_MAX) {
	DEBUG("too large read: %zu bytes", size);
	return (EIO);
    }

    blks = size / BD(dev).bd_sectorsize;
    if (dblk > dblk + blks)
	return (EIO);

    if (rsize)
	*rsize = 0;

    /* Get disk blocks, this value is either for whole disk or for partition */
    if (disk_ioctl(dev, DIOCGMEDIASIZE, &disk_blocks)) {
	/* DIOCGMEDIASIZE does return bytes. */
        disk_blocks /= BD(dev).bd_sectorsize;
    } else {
	/* We should not get here. Just try to survive. */
	disk_blocks = BD(dev).bd_sectors - dev->d_offset;
    }

    /* Validate source block address. */
    if (dblk < dev->d_offset || dblk >= dev->d_offset + disk_blocks)
	return (EIO);

    /*
     * Truncate if we are crossing disk or partition end.
     */
    if (dblk + blks >= dev->d_offset + disk_blocks) {
	blks = dev->d_offset + disk_blocks - dblk;
	size = blks * BD(dev).bd_sectorsize;
	DEBUG("short read %d", blks);
    }

    switch (rw & F_MASK) {
    case F_READ:
	DEBUG("read %d from %lld to %p", blks, dblk, buf);

	if (blks && (rc = bd_read(dev, dblk, blks, buf))) {
	    /* Filter out floppy controller errors */
	    if (BD(dev).bd_flags != BD_FLOPPY || rc != 0x20) {
		printf("read %d from %lld to %p, error: 0x%x", blks, dblk,
		    buf, rc);
	    }
	    return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS /* XXX: sector size */
	DEBUG("bd_strategy: frag read %d from %d+%d to %p",
	    fragsize, dblk, blks, buf + (blks * BIOSDISK_SECSIZE));
	if (fragsize && bd_read(od, dblk + blks, 1, fragsize)) {
	    DEBUG("frag read error");
	    return(EIO);
	}
	bcopy(fragbuf, buf + (blks * BIOSDISK_SECSIZE), fragsize);
#endif
	break;
    case F_WRITE :
	DEBUG("write %d from %d to %p", blks, dblk, buf);

	if (blks && bd_write(dev, dblk, blks, buf)) {
	    DEBUG("write error");
	    return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS
	if(fragsize) {
	    DEBUG("Attempted to write a frag");
	    return (EIO);
	}
#endif
	break;
    default:
	/* DO NOTHING */
	return (EROFS);
    }

    if (rsize)
	*rsize = size;
    return (0);
}

static int
bd_edd_io(struct disk_devdesc *dev, daddr_t dblk, int blks, caddr_t dest,
    int write)
{
    static struct edd_packet packet;

    packet.len = sizeof(struct edd_packet);
    packet.count = blks;
    packet.off = VTOPOFF(dest);
    packet.seg = VTOPSEG(dest);
    packet.lba = dblk;
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    if (write)
	/* Should we Write with verify ?? 0x4302 ? */
	v86.eax = 0x4300;
    else
	v86.eax = 0x4200;
    v86.edx = BD(dev).bd_unit;
    v86.ds = VTOPSEG(&packet);
    v86.esi = VTOPOFF(&packet);
    v86int();
    if (V86_CY(v86.efl))
	return (v86.eax >> 8);
    return (0);
}

static int
bd_chs_io(struct disk_devdesc *dev, daddr_t dblk, int blks, caddr_t dest,
    int write)
{
    u_int	x, bpc, cyl, hd, sec;

    bpc = BD(dev).bd_sec * BD(dev).bd_hds;	/* blocks per cylinder */
    x = dblk;
    cyl = x / bpc;			/* block # / blocks per cylinder */
    x %= bpc;				/* block offset into cylinder */
    hd = x / BD(dev).bd_sec;		/* offset / blocks per track */
    sec = x % BD(dev).bd_sec;		/* offset into track */

    /* correct sector number for 1-based BIOS numbering */
    sec++;

    if (cyl > 1023)
	/* CHS doesn't support cylinders > 1023. */
	return (1);

    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    if (write)
	v86.eax = 0x300 | blks;
    else
	v86.eax = 0x200 | blks;
    v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
    v86.edx = (hd << 8) | BD(dev).bd_unit;
    v86.es = VTOPSEG(dest);
    v86.ebx = VTOPOFF(dest);
    v86int();
    if (V86_CY(v86.efl))
	return (v86.eax >> 8);
    return (0);
}

static int
bd_io(struct disk_devdesc *dev, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, sec, result, resid, retry, maxfer;
    caddr_t	p, xp, bbuf;
    
    /* Just in case some idiot actually tries to read/write -1 blocks... */
    if (blks < 0)
	return (-1);

    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
    if (VTOP(dest) >> 20 != 0 || (BD(dev).bd_unit < 0x80 &&
	(VTOP(dest) >> 16) != (VTOP(dest +
	blks * BD(dev).bd_sectorsize) >> 16))) {

	/* 
	 * There is a 64k physical boundary somewhere in the
	 * destination buffer, or the destination buffer is above
	 * first 1MB of physical memory so we have to arrange a
	 * suitable bounce buffer.  Allocate a buffer twice as large
	 * as we need to.  Use the bottom half unless there is a break
	 * there, in which case we use the top half.
	 */
	x = V86_IO_BUFFER_SIZE / BD(dev).bd_sectorsize;
	x = min(x, (unsigned)blks);
	bbuf = PTOV(V86_IO_BUFFER);
	maxfer = x;		/* limit transfers to bounce region size */
    } else {
	bbuf = NULL;
	maxfer = 0;
    }
    
    while (resid > 0) {
	/*
	 * Play it safe and don't cross track boundaries.
	 * (XXX this is probably unnecessary)
	 */
	sec = dblk % BD(dev).bd_sec;	/* offset into track */
	x = min(BD(dev).bd_sec - sec, resid);
	if (maxfer > 0)
	    x = min(x, maxfer);		/* fit bounce buffer */

	/* where do we transfer to? */
	xp = bbuf == NULL ? p : bbuf;

	/*
	 * Put your Data In, Put your Data out,
	 * Put your Data In, and shake it all about 
	 */
	if (write && bbuf != NULL)
	    bcopy(p, bbuf, x * BD(dev).bd_sectorsize);

	/*
	 * Loop retrying the operation a couple of times.  The BIOS
	 * may also retry.
	 */
	for (retry = 0; retry < 3; retry++) {
	    /* if retrying, reset the drive */
	    if (retry > 0) {
		v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0;
		v86.edx = BD(dev).bd_unit;
		v86int();
	    }

	    if (BD(dev).bd_flags & BD_MODEEDD1)
		result = bd_edd_io(dev, dblk, x, xp, write);
	    else
		result = bd_chs_io(dev, dblk, x, xp, write);
	    if (result == 0)
		break;
	}

	if (write)
	    DEBUG("Write %d sector(s) from %p (0x%x) to %lld %s", x,
		p, VTOP(p), dblk, result ? "failed" : "ok");
	else
	    DEBUG("Read %d sector(s) from %lld to %p (0x%x) %s", x,
		dblk, p, VTOP(p), result ? "failed" : "ok");
	if (result) {
	    return (result);
	}
	if (!write && bbuf != NULL)
	    bcopy(bbuf, p, x * BD(dev).bd_sectorsize);
	p += (x * BD(dev).bd_sectorsize);
	dblk += x;
	resid -= x;
    }

/*    hexdump(dest, (blks * BD(dev).bd_sectorsize)); */
    return(0);
}

static int
bd_read(struct disk_devdesc *dev, daddr_t dblk, int blks, caddr_t dest)
{
#ifdef LOADER_GELI_SUPPORT
	struct dsk dskp;
	off_t p_off, diff;
	daddr_t alignlba;
	int err, n, alignblks;
	char *tmpbuf;

	/* if we already know there is no GELI, skip the rest */
	if (geli_status[dev->dd.d_unit][dev->d_slice] != ISGELI_YES)
		return (bd_io(dev, dblk, blks, dest, 0));

	if (geli_status[dev->dd.d_unit][dev->d_slice] == ISGELI_YES) {
		/*
		 * Align reads to DEV_GELIBOOT_BSIZE bytes because partial
		 * sectors cannot be decrypted. Round the requested LBA down to
		 * nearest multiple of DEV_GELIBOOT_BSIZE bytes.
		 */
		alignlba = rounddown2(dblk * BD(dev).bd_sectorsize,
		    DEV_GELIBOOT_BSIZE) / BD(dev).bd_sectorsize;
		/*
		 * Round number of blocks to read up to nearest multiple of
		 * DEV_GELIBOOT_BSIZE
		 */
		diff = (dblk - alignlba) * BD(dev).bd_sectorsize;
		alignblks = roundup2(blks * BD(dev).bd_sectorsize + diff,
		    DEV_GELIBOOT_BSIZE) / BD(dev).bd_sectorsize;

		/*
		 * If the read is rounded up to a larger size, use a temporary
		 * buffer here because the buffer provided by the caller may be
		 * too small.
		 */
		if (diff == 0) {
			tmpbuf = dest;
		} else {
			tmpbuf = malloc(alignblks * BD(dev).bd_sectorsize);
			if (tmpbuf == NULL) {
				return (-1);
			}
		}

		err = bd_io(dev, alignlba, alignblks, tmpbuf, 0);
		if (err)
			return (err);

		dskp.drive = bd_unit2bios(dev->dd.d_unit);
		dskp.type = dev->dd.d_dev->dv_type;
		dskp.unit = dev->dd.d_unit;
		dskp.slice = dev->d_slice;
		dskp.part = dev->d_partition;
		dskp.start = dev->d_offset;

		/* GELI needs the offset relative to the partition start */
		p_off = alignlba - dskp.start;

		err = geli_read(&dskp, p_off * BD(dev).bd_sectorsize, (u_char *)tmpbuf,
		    alignblks * BD(dev).bd_sectorsize);
		if (err)
			return (err);

		if (tmpbuf != dest) {
			bcopy(tmpbuf + diff, dest, blks * BD(dev).bd_sectorsize);
			free(tmpbuf);
		}
		return (0);
	}
#endif /* LOADER_GELI_SUPPORT */

	return (bd_io(dev, dblk, blks, dest, 0));
}

static int
bd_write(struct disk_devdesc *dev, daddr_t dblk, int blks, caddr_t dest)
{

	return (bd_io(dev, dblk, blks, dest, 1));
}

/*
 * Return the BIOS geometry of a given "fixed drive" in a format
 * suitable for the legacy bootinfo structure.  Since the kernel is
 * expecting raw int 0x13/0x8 values for N_BIOS_GEOM drives, we
 * prefer to get the information directly, rather than rely on being
 * able to put it together from information already maintained for
 * different purposes and for a probably different number of drives.
 *
 * For valid drives, the geometry is expected in the format (31..0)
 * "000000cc cccccccc hhhhhhhh 00ssssss"; and invalid drives are
 * indicated by returning the geometry of a "1.2M" PC-format floppy
 * disk.  And, incidentally, what is returned is not the geometry as
 * such but the highest valid cylinder, head, and sector numbers.
 */
u_int32_t
bd_getbigeom(int bunit)
{

    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = 0x80 + bunit;
    v86int();
    if (V86_CY(v86.efl))
	return 0x4f010f;
    return ((v86.ecx & 0xc0) << 18) | ((v86.ecx & 0xff00) << 8) |
	   (v86.edx & 0xff00) | (v86.ecx & 0x3f);
}

/*
 * Return a suitable dev_t value for (dev).
 *
 * In the case where it looks like (dev) is a SCSI disk, we allow the number of
 * IDE disks to be specified in $num_ide_disks.  There should be a Better Way.
 */
int
bd_getdev(struct i386_devdesc *d)
{
    struct disk_devdesc		*dev;
    int				biosdev;
    int 			major;
    int				rootdev;
    char			*nip, *cp;
    int				i, unit;

    dev = (struct disk_devdesc *)d;
    biosdev = bd_unit2bios(dev->dd.d_unit);
    DEBUG("unit %d BIOS device %d", dev->dd.d_unit, biosdev);
    if (biosdev == -1)				/* not a BIOS device */
	return(-1);
    if (disk_open(dev, BD(dev).bd_sectors * BD(dev).bd_sectorsize,
	BD(dev).bd_sectorsize) != 0)		/* oops, not a viable device */
	    return (-1);
    else
	disk_close(dev);

    if (biosdev < 0x80) {
	/* floppy (or emulated floppy) or ATAPI device */
	if (bdinfo[dev->dd.d_unit].bd_type == DT_ATAPI) {
	    /* is an ATAPI disk */
	    major = WFDMAJOR;
	} else {
	    /* is a floppy disk */
	    major = FDMAJOR;
	}
    } else {
	    /* assume an IDE disk */
	    major = WDMAJOR;
    }
    /* default root disk unit number */
    unit = biosdev & 0x7f;

    /* XXX a better kludge to set the root disk unit number */
    if ((nip = getenv("root_disk_unit")) != NULL) {
	i = strtol(nip, &cp, 0);
	/* check for parse error */
	if ((cp != nip) && (*cp == 0))
	    unit = i;
    }

    rootdev = MAKEBOOTDEV(major, dev->d_slice + 1, unit, dev->d_partition);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}

#ifdef LOADER_GELI_SUPPORT
int
bios_read(void *vdev __unused, void *xpriv, off_t off, void *buf, size_t bytes)
{
	struct disk_devdesc dev;
	struct dsk *priv = xpriv;

	dev.dd.d_dev = &biosdisk;
	dev.dd.d_unit = priv->unit;
	dev.d_slice = priv->slice;
	dev.d_partition = priv->part;
	dev.d_offset = priv->start;

	off = off / BD(&dev).bd_sectorsize;
	/* GELI gives us the offset relative to the partition start */
	off += dev.d_offset;
	bytes = bytes / BD(&dev).bd_sectorsize;

	return (bd_io(&dev, off, bytes, buf, 0));
}
#endif /* LOADER_GELI_SUPPORT */
