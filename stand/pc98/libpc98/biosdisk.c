/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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

#include <sys/disklabel.h>
#include <sys/diskpc98.h>

#include <bootstrap.h>
#include <btxv86.h>
#include "disk.h"
#include "libi386.h"

#ifdef LOADER_GELI_SUPPORT
#error "Nope! No GELI on pc98 so sorry."
#endif

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
#define BD_MODEINT13		0x0000
#define BD_MODEEDD1		0x0001
#define BD_MODEEDD3		0x0002
#define BD_MODEMASK		0x0003
#define BD_FLOPPY		0x0004
#define BD_LABELOK		0x0008
#define BD_PARTTABOK		0x0010
#define BD_OPTICAL		0x0020
	int		bd_type;	/* BIOS 'drive type' (floppy only) */
	uint16_t	bd_sectorsize;	/* Sector size */
	uint64_t	bd_sectors;	/* Disk size */
	int		bd_da_unit;	/* kernel unit number for da */
	int		bd_open;	/* reference counter */
	void		*bd_bcache;	/* buffer cache data */
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

#define	BD(dev)	(bdinfo[(dev)->dd.d_unit])

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
	int base, unit;
	int da_drive=0, n=-0x10;

	/* sequence 0x90, 0x80, 0xa0 */
	for (base = 0x90; base <= 0xa0; base += n, n += 0x30) {
		for (unit = base; (nbdinfo < MAXBDDEV) || ((unit & 0x0f) < 4);
		     unit++) {
			bdinfo[nbdinfo].bd_open = 0;
			bdinfo[nbdinfo].bd_bcache = NULL;
			bdinfo[nbdinfo].bd_unit = unit;
			bdinfo[nbdinfo].bd_flags =
				(unit & 0xf0) == 0x90 ? BD_FLOPPY : 0;
			if (!bd_int13probe(&bdinfo[nbdinfo])) {
				if (((unit & 0xf0) == 0x90 &&
					(unit & 0x0f) < 4) ||
				    ((unit & 0xf0) == 0xa0 &&
					(unit & 0x0f) < 6))
					/* Target IDs are not contiguous. */
					continue;
				else
					break;
			}

			if (bdinfo[nbdinfo].bd_flags & BD_FLOPPY) {
				/* available 1.44MB access? */
				if (*(u_char *)PTOV(0xA15AE) &
				    (1<<(unit & 0xf))) {
					/* boot media 1.2MB FD? */
					if ((*(u_char *)PTOV(0xA1584) &
						0xf0) != 0x90)
						bdinfo[nbdinfo].bd_unit =
							0x30 + (unit & 0xf);
				}
			} else {
				if ((unit & 0xF0) == 0xA0) /* SCSI HD or MO */
					bdinfo[nbdinfo].bd_da_unit =
						da_drive++;
			}
			/* XXX we need "disk aliases" to make this simpler */
			printf("BIOS drive %c: is disk%d\n",
			    'A' + nbdinfo, nbdinfo);
			nbdinfo++;
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
	int addr;

	if (bd->bd_flags & BD_FLOPPY) {
		addr = 0xa155c;
	} else {
		if ((bd->bd_unit & 0xf0) == 0x80)
			addr = 0xa155d;
		else
			addr = 0xa1482;
	}
	if ( *(u_char *)PTOV(addr) & (1<<(bd->bd_unit & 0x0f))) {
		bd->bd_flags |= BD_MODEINT13;
		return (1);
	}
	if ((bd->bd_unit & 0xF0) == 0xA0) {
		int media =
			((unsigned *)PTOV(0xA1460))[bd->bd_unit & 0x0F] & 0x1F;

		if (media == 7) { /* MO */
			bd->bd_flags |= BD_MODEINT13 | BD_OPTICAL;
			return(1);
		}
	}
	return (0);
}

/*
 * Print information about disks
 */
static int
bd_print(int verbose)
{
	char line[80];
	struct disk_devdesc dev;
	int i, ret = 0;
	struct pc98_partition *dptr;
    
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

		/* try to open the whole disk */
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

/* Given a size in 512 byte sectors, convert it to a human-readable number. */
static char *
display_size(uint64_t size)
{
	static char buf[80];
	char unit;

	size /= 2;
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
	sprintf(buf, "%6ld%cB", (long)size, unit);
	return (buf);
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
	va_list				ap;
	struct disk_devdesc		*dev;
	struct disk_devdesc		disk;
	int				err;
	uint64_t			size;

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
	/* i386 has GELI here */
	return(err);
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

/* Max number of sectors to bounce-buffer if the request crosses a 64k boundary */
#define FLOPPY_BOUNCEBUF	18

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

    v86.ctl = V86_FLAGS;
    v86.addr = 0x1b;
    if (write)
        v86.eax = 0x0500 | BD(dev).bd_unit;
    else
	v86.eax = 0x0600 | BD(dev).bd_unit;
    if (BD(dev).bd_flags & BD_FLOPPY) {
	v86.eax |= 0xd000;
	v86.ecx = 0x0200 | (cyl & 0xff);
	v86.edx = (hd << 8) | (sec + 1);
    } else if (BD(dev).bd_flags & BD_OPTICAL) {
	v86.eax &= 0xFF7F;
	v86.ecx = dblk & 0xFFFF;
	v86.edx = dblk >> 16;
    } else {
	v86.ecx = cyl;
	v86.edx = (hd << 8) | sec;
    }
    v86.ebx = blks * BIOSDISK_SECSIZE;
    v86.es = VTOPSEG(dest);
    v86.ebp = VTOPOFF(dest);
    v86int();
    return (V86_CY(v86.efl));
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
		v86.addr = 0x1b;
		v86.eax = 0x0300 | BD(dev).bd_unit;
		v86int();
	    }

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
bd_read(struct disk_devdesc *dev, daddr_t dblk, int blks,
    caddr_t dest)
{
	/* i386 has GELI here */

	return (bd_io(dev, dblk, blks, dest, 0));
}

static int
bd_write(struct disk_devdesc *dev, daddr_t dblk, int blks,
    caddr_t dest)
{

	return (bd_io(dev, dblk, blks, dest, 1));
}

#if 0
static int
bd_getgeom(struct open_disk *od)
{

    if (od->od_flags & BD_FLOPPY) {
	od->od_cyl = 79;
	od->od_hds = 2;
	od->od_sec = (od->od_unit & 0xf0) == 0x30 ? 18 : 15;
    } else if (od->od_flags & BD_OPTICAL) {
	od->od_cyl = 0xFFFE;
	od->od_hds = 8;
	od->od_sec = 32;
    } else {
	v86.ctl = V86_FLAGS;
	v86.addr = 0x1b;
	v86.eax = 0x8400 | od->od_unit;
	v86int();
      
	od->od_cyl = v86.ecx;
	od->od_hds = (v86.edx >> 8) & 0xff;
	od->od_sec = v86.edx & 0xff;
	if (V86_CY(v86.efl))
	    return(1);
    }

    DEBUG("unit 0x%x geometry %d/%d/%d", od->od_unit, od->od_cyl, od->od_hds, od->od_sec);
    return(0);
}
#endif

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
    int hds = 0;
    int unit = 0x80;		/* IDE HDD */
    u_int addr = 0xA155d;

    while (unit < 0xa7) {
	if (*(u_char *)PTOV(addr) & (1 << (unit & 0x0f)))
	    if (hds++ == bunit)
		break;

	if (unit >= 0xA0) {
	    int  media = ((unsigned *)PTOV(0xA1460))[unit & 0x0F] & 0x1F;

	    if (media == 7 && hds++ == bunit)	/* SCSI MO */
		return(0xFFFE0820); /* C:65535 H:8 S:32 */
	}
	if (++unit == 0x84) {
	    unit = 0xA0;	/* SCSI HDD */
	    addr = 0xA1482;
	}
    }
    if (unit == 0xa7)
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    v86.ctl = V86_FLAGS;
    v86.addr = 0x1b;
    v86.eax = 0x8400 | unit;
    v86int();
    if (V86_CY(v86.efl))
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    return ((v86.ecx & 0xffff) << 16) | (v86.edx & 0xffff);
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
    int				unitofs = 0, i, unit;

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

    if ((biosdev & 0xf0) == 0x90 || (biosdev & 0xf0) == 0x30) {
	/* floppy (or emulated floppy) or ATAPI device */
	if (BD(dev).bd_type == DT_ATAPI) {
	    /* is an ATAPI disk */
	    major = WFDMAJOR;
	} else {
	    /* is a floppy disk */
	    major = FDMAJOR;
	}
    } else {
	/* harddisk */
	if ((BD(dev).bd_flags & BD_LABELOK) && 0) {
//	    (BD(dev).bd_disklabel.d_type == DTYPE_SCSI)) {
	    /* label OK, disk labelled as SCSI */
	    major = DAMAJOR;
	    /* check for unit number correction hint, now deprecated */
	    if ((nip = getenv("num_ide_disks")) != NULL) {
		i = strtol(nip, &cp, 0);
		/* check for parse error */
		if ((cp != nip) && (*cp == 0))
		    unitofs = i;
	    }
	} else {
	    /* assume an IDE disk */
	    major = WDMAJOR;
	}
    }
    /* default root disk unit number */
    if ((biosdev & 0xf0) == 0xa0)
	unit = BD(dev).bd_da_unit;
    else
	unit = biosdev & 0xf;

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
